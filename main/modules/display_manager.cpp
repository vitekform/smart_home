#include "display_manager.h"
#include "modules/state_manager.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

static uint8_t oled_buffer[128 * 64 / 8];

DisplayManager& DisplayManager::get_instance() {
    static DisplayManager instance;
    return instance;
}

void DisplayManager::init(gpio_num_t sda_pin, gpio_num_t scl_pin) {
    if (initialized) {
        return;
    }

    std::cout << "[LVGL] Initializing I2C bus on SDA:" << sda_pin << ", SCL:" << scl_pin << "..." << std::endl;

    // 1. Configure the I2C Master Bus
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = sda_pin;
    bus_config.scl_io_num = scl_pin;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK) {
        std::cerr << "[LVGL] Failed to initialize I2C bus: " << esp_err_to_name(err) << std::endl;
        return;
    }

    // 2. Scan the I2C bus to find the display's address and confirm electrical connectivity
    std::cout << "[LVGL] Probing I2C bus..." << std::endl;
    uint8_t oled_addr = 0x3C;
    bool found = false;

    if (i2c_master_probe(i2c_bus, 0x3C, 50) == ESP_OK) {
        std::cout << "[LVGL] Detected SSD1306 on address 0x3C" << std::endl;
        oled_addr = 0x3C;
        found = true;
    } else if (i2c_master_probe(i2c_bus, 0x3D, 50) == ESP_OK) {
        std::cout << "[LVGL] Detected SSD1306 on address 0x3D" << std::endl;
        oled_addr = 0x3D;
        found = true;
    } else {
        std::cerr << "[LVGL] No SSD1306 detected on default addresses (0x3C or 0x3D)!" << std::endl;
        std::cerr << "[LVGL] Scanning all I2C addresses 0x00-0x7F..." << std::endl;
        for (uint8_t i = 1; i < 127; i++) {
            if (i2c_master_probe(i2c_bus, i, 50) == ESP_OK) {
                std::cout << "[LVGL SCAN] Found I2C device at address: 0x" << std::hex << (int)i << std::dec << std::endl;
                oled_addr = i;
                found = true;
            }
        }
    }

    if (!found) {
        std::cerr << "[LVGL] Warning: No I2C devices responded to probe. Screen may not be powered or wired incorrectly." << std::endl;
    }

    // 3. Configure the Panel IO
    esp_lcd_panel_io_handle_t io_hdl = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {};
    io_config.dev_addr = oled_addr;
    io_config.scl_speed_hz = 100000; // 100kHz for signal integrity
    io_config.control_phase_bytes = 1;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.dc_bit_offset = 6;

    err = esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_hdl);
    if (err != ESP_OK) {
        std::cerr << "[LVGL] Failed to install panel IO: " << esp_err_to_name(err) << std::endl;
        return;
    }
    io_handle = io_hdl;

    // 4. Configure the SSD1306 panel driver
    esp_lcd_panel_handle_t pan_hdl = NULL;
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.bits_per_pixel = 1;
    panel_config.reset_gpio_num = GPIO_NUM_NC;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {};
    ssd1306_config.height = 64;

    panel_config.vendor_config = &ssd1306_config;
    err = esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &pan_hdl);
    if (err != ESP_OK) {
        std::cerr << "[LVGL] Failed to install SSD1306 panel driver: " << esp_err_to_name(err) << std::endl;
        return;
    }
    panel_handle = pan_hdl;

    // Reset, Init, and Turn Display On
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 5. Initialize LVGL
    lv_init();

    // Create a display
    display = lv_display_create(128, 64);
    lv_display_set_default(display);
    lv_display_set_user_data(display, panel_handle);

    // Allocate LVGL draw buffer (including 8 bytes palette)
    size_t draw_buffer_sz = 128 * 64 / 8 + 8;
    draw_buf = malloc(draw_buffer_sz);
    if (draw_buf == nullptr) {
        std::cerr << "[LVGL] Failed to allocate draw buffer!" << std::endl;
        return;
    }

    // Set format, buffers, and flush callback
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display, draw_buf, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    // Register io panel event callback for LVGL flush ready notification
    esp_lcd_panel_io_callbacks_t cbs = {};
    cbs.on_color_trans_done = example_notify_lvgl_flush_ready;
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display);

    // 6. Use esp_timer for LVGL tick
    esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &example_increase_lvgl_tick;
    lvgl_tick_timer_args.arg = nullptr;
    lvgl_tick_timer_args.name = "lvgl_tick";
    lvgl_tick_timer_args.skip_unhandled_events = true;

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 5 * 1000)); // 5ms

    // Create UI layout
    std::lock_guard<std::mutex> lock(lvgl_mutex);
    
    // Clear screen conversion buffer initially
    std::memset(oled_buffer, 0, sizeof(oled_buffer));

    // Create text labels on the display's active screen
    lv_obj_t *screen = lv_display_get_screen_active(display);
    label_header = lv_label_create(screen);
    label_divider = lv_label_create(screen);
    label_wifi = lv_label_create(screen);
    label_mqtt = lv_label_create(screen);
    label_slaves = lv_label_create(screen);
    label_meteos = lv_label_create(screen);
    label_sensor = lv_label_create(screen);

    // Position them on the screen (Montserrat 8 font, height 8px)
    lv_obj_align(label_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align(label_divider, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_align(label_wifi, LV_ALIGN_TOP_LEFT, 0, 17);
    lv_obj_align(label_mqtt, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_align(label_slaves, LV_ALIGN_TOP_LEFT, 0, 35);
    lv_obj_align(label_meteos, LV_ALIGN_TOP_LEFT, 0, 44);
    lv_obj_align(label_sensor, LV_ALIGN_TOP_LEFT, 0, 53);

    // Initial label text
    lv_label_set_text(label_header, "*** MASTER NODE ***");
    lv_label_set_text(label_divider, "--------------------");
    lv_label_set_text(label_wifi, "WiFi:  Init...");
    lv_label_set_text(label_mqtt, "MQTT:  Init...");
    lv_label_set_text(label_slaves, "Slaves: 0");
    lv_label_set_text(label_meteos, "Meteos: 0");
    lv_label_set_text(label_sensor, "Temp:  N/A | Hum: N/A");

    initialized = true;
    std::cout << "[LVGL] Display initialized successfully with LVGL9!" << std::endl;
}

void DisplayManager::start() {
    if (!initialized) {
        std::cerr << "[LVGL] Cannot start: Not initialized." << std::endl;
        return;
    }
    // Create task to handle periodic UI content updates
    xTaskCreate(display_update_task, "display_update_task", 4096, nullptr, 5, nullptr);
    
    // Create task to run LVGL tick/timer handler
    xTaskCreate(lvgl_port_task, "lvgl_port_task", 4096, nullptr, 5, nullptr);

    // Create task to force full screen re-render every 5 seconds
    xTaskCreate(screen_invalidate_task, "screen_invalidate_task", 2048, nullptr, 5, nullptr);
}

void DisplayManager::lock() {
    lvgl_mutex.lock();
}

void DisplayManager::unlock() {
    lvgl_mutex.unlock();
}

void DisplayManager::example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(5); // 5ms tick
}

void DisplayManager::lvgl_port_task(void* pvParameters) {
    auto& dm = DisplayManager::get_instance();
    while (true) {
        dm.lock();
        uint32_t delay = lv_timer_handler();
        dm.unlock();

        if (delay < 10) {
            delay = 10;
        } else if (delay > 30) {
            delay = 30;
        }
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

void DisplayManager::screen_invalidate_task(void* pvParameters) {
    auto& dm = DisplayManager::get_instance();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (dm.initialized) {
            dm.lock();
            lv_obj_invalidate(lv_display_get_screen_active(dm.display));
            dm.unlock();
        }
    }
}

void DisplayManager::example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel_hdl = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    // Skip palette (first 8 bytes)
    px_map += 8;

    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & (1 << (7 - x % 8)));
            uint8_t *buf = oled_buffer + hor_res * (y >> 3) + x;
            if (chroma_color) {
                (*buf) &= ~(1 << (y % 8));
            } else {
                (*buf) |= (1 << (y % 8));
            }
        }
    }

    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_hdl, x1, y1, x2 + 1, y2 + 1, oled_buffer);
    if (ret != ESP_OK) {
        std::cerr << "[LVGL CALLBACK] esp_lcd_panel_draw_bitmap failed: " << esp_err_to_name(ret) << std::endl;
        // Fallback: notify ready to prevent rendering engine freeze
        lv_display_flush_ready(disp);
    }
}

bool DisplayManager::example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

void DisplayManager::display_update_task(void* pvParameters) {
    auto& state = SystemStateManager::get_instance();
    auto& dm = DisplayManager::get_instance();

    while (true) {
        if (!dm.initialized) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Get states
        std::string wifi_str = "WiFi:  ";
        wifi_str += (state.is_wifi_connected() ? "CONNECTED" : "DISCONNECTED");

        std::string mqtt_str = "MQTT:  ";
        mqtt_str += (state.is_mqtt_connected() ? "CONNECTED" : "DISCONNECTED");

        int slaves = state.get_slave_uuids().size();
        int meteos = state.get_meteo_uuids().size();
        
        std::stringstream ss_slaves;
        ss_slaves << "Slaves: " << slaves;

        std::stringstream ss_meteos;
        ss_meteos << "Meteos: " << meteos;

        float temp = 0.0f;
        float hum = 0.0f;
        state.get_sensor_data(temp, hum);

        std::stringstream ss_sensor;
        if (meteos > 0 && (temp != 0.0f || hum != 0.0f)) {
            ss_sensor << "Temp:  " << std::fixed << std::setprecision(1) << temp << "C | Hum: " 
                      << std::fixed << std::setprecision(1) << hum << "%";
        } else {
            ss_sensor << "Temp:  N/A | Hum: N/A";
        }

        // Update UI labels (thread-safe)
        dm.lock();
        lv_label_set_text(dm.label_wifi, wifi_str.c_str());
        lv_label_set_text(dm.label_mqtt, mqtt_str.c_str());
        lv_label_set_text(dm.label_slaves, ss_slaves.str().c_str());
        lv_label_set_text(dm.label_meteos, ss_meteos.str().c_str());
        lv_label_set_text(dm.label_sensor, ss_sensor.str().c_str());
        dm.unlock();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
