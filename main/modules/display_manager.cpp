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

// ── Pixel row positions for the 7 label slots (Montserrat 8, 8 px tall) ──────
static const int ROW_Y[7] = {0, 8, 17, 26, 35, 44, 53};

DisplayManager& DisplayManager::get_instance() {
    static DisplayManager instance;
    return instance;
}

// ─────────────────────────────────────────────────────────────────────────────
//  init
// ─────────────────────────────────────────────────────────────────────────────
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
                std::cout << "[LVGL SCAN] Found I2C device at address: 0x"
                          << std::hex << (int)i << std::dec << std::endl;
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

    // 7. Create UI layout — 7 generic label rows
    std::lock_guard<std::mutex> lock(lvgl_mutex);
    std::memset(oled_buffer, 0, sizeof(oled_buffer));

    lv_obj_t *screen = lv_display_get_screen_active(display);
    for (int i = 0; i < 7; i++) {
        label_row[i] = lv_label_create(screen);
        lv_obj_align(label_row[i], LV_ALIGN_TOP_LEFT, 0, ROW_Y[i]);
        lv_label_set_text(label_row[i], "");
    }

    // Render BOOTING splash — shown while the rest of the system initialises
    current_screen = ScreenID::BOOTING;
    list_scroll_offset = 0;

    lv_obj_set_align(label_row[0], LV_ALIGN_TOP_MID);
    lv_obj_set_align(label_row[1], LV_ALIGN_TOP_MID);
    lv_obj_set_pos(label_row[0], 0, ROW_Y[0]);
    lv_obj_set_pos(label_row[1], 0, ROW_Y[1]);

    lv_label_set_text(label_row[0], "*** MASTER NODE ***");
    lv_label_set_text(label_row[1], "--------------------");
    lv_label_set_text(label_row[2], "");
    lv_label_set_text(label_row[3], "  Booting up...");
    lv_label_set_text(label_row[4], "");
    lv_label_set_text(label_row[5], "");
    lv_label_set_text(label_row[6], "");
    lv_obj_invalidate(lv_display_get_screen_active(display));

    initialized = true;
    std::cout << "[LVGL] Display initialized successfully with LVGL9!" << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
//  start
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::start() {
    if (!initialized) {
        std::cerr << "[LVGL] Cannot start: Not initialized." << std::endl;
        return;
    }
    xTaskCreate(display_update_task,    "display_update_task",    4096, nullptr, 5, nullptr);
    xTaskCreate(lvgl_port_task,         "lvgl_port_task",         4096, nullptr, 5, nullptr);
    xTaskCreate(screen_invalidate_task, "screen_invalidate_task", 2048, nullptr, 5, nullptr);
}

// ───────────────────────────────────────────────────────────────────────────────
//  show_boot_screen
//  May be called any time after init() to jump back to the BOOTING splash.
// ───────────────────────────────────────────────────────────────────────────────
void DisplayManager::show_boot_screen() {
    if (!initialized) return;
    spinner_frame = 0;
    current_screen = ScreenID::BOOTING;
    list_scroll_offset = 0;
    render_screen(ScreenID::BOOTING);
}

// ───────────────────────────────────────────────────────────────────────────────
//  show_shutdown_screen
//  Shows the shutdown splash and blocks ~1.5 s so the message is visible
//  before the caller invokes esp_restart().
// ───────────────────────────────────────────────────────────────────────────────
void DisplayManager::show_shutdown_screen() {
    if (!initialized) return;
    spinner_frame = 0;
    current_screen = ScreenID::SHUTDOWN;
    list_scroll_offset = 0;
    render_screen(ScreenID::SHUTDOWN);
    // Hold for ~2 s so the spinner can animate visibly before the restart fires
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// ───────────────────────────────────────────────────────────────────────────────
//  transition_to_status
//  Called once the system is fully booted to leave the BOOTING splash and start
//  the normal STATUS screen cycle.
// ───────────────────────────────────────────────────────────────────────────────
void DisplayManager::transition_to_status() {
    if (!initialized) return;
    current_screen = ScreenID::STATUS;
    list_scroll_offset = 0;
    render_screen(ScreenID::STATUS);
}

void DisplayManager::lock()   { lvgl_mutex.lock(); }
void DisplayManager::unlock() { lvgl_mutex.unlock(); }

// ─────────────────────────────────────────────────────────────────────────────
//  navigate  — called from display_update_task with the button event
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::navigate(ButtonEvent event) {
    int idx = static_cast<int>(current_screen);

    switch (event) {
        case ButtonEvent::RIGHT:
            idx = (idx + 1) % SCREEN_COUNT;
            list_scroll_offset = 0;
            current_screen = static_cast<ScreenID>(idx);
            render_screen(current_screen);
            break;

        case ButtonEvent::LEFT:
            idx = (idx - 1 + SCREEN_COUNT) % SCREEN_COUNT;
            list_scroll_offset = 0;
            current_screen = static_cast<ScreenID>(idx);
            render_screen(current_screen);
            break;

        case ButtonEvent::BACK:
            list_scroll_offset = 0;
            current_screen = ScreenID::STATUS;
            render_screen(current_screen);
            break;

        case ButtonEvent::DOWN:
            if (current_screen == ScreenID::SLAVES || current_screen == ScreenID::METEOS) {
                list_scroll_offset++;
                render_screen(current_screen);
            }
            break;

        case ButtonEvent::UP:
            if (current_screen == ScreenID::SLAVES || current_screen == ScreenID::METEOS) {
                if (list_scroll_offset > 0) {
                    list_scroll_offset--;
                    render_screen(current_screen);
                }
            }
            break;

        case ButtonEvent::SELECT:
            // Reserved for future use (enter detail view, etc.)
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  render_screen  — updates all 7 label rows for the given screen
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::render_screen(ScreenID screen) {
    auto& state = SystemStateManager::get_instance();

    // Helper lambda: set all 7 rows at once
    auto set_rows = [&](const char* r0, const char* r1,
                        const char* r2, const char* r3,
                        const char* r4, const char* r5,
                        const char* r6)
    {
        lock();
        lv_label_set_text(label_row[0], r0);
        lv_label_set_text(label_row[1], r1);
        lv_label_set_text(label_row[2], r2);
        lv_label_set_text(label_row[3], r3);
        lv_label_set_text(label_row[4], r4);
        lv_label_set_text(label_row[5], r5);
        lv_label_set_text(label_row[6], r6);
        lv_obj_invalidate(lv_display_get_screen_active(display));
        unlock();
    };

    switch (screen) {

        // ── Screen 0: STATUS ────────────────────────────────────────────────
        case ScreenID::STATUS: {
            std::string wifi_str = state.is_wifi_connected() ? "WiFi:  CONNECTED"
                                                             : "WiFi:  DISCONNECTED";
            std::string mqtt_str = state.is_mqtt_connected() ? "MQTT:  CONNECTED"
                                                             : "MQTT:  DISCONNECTED";
            int slaves = static_cast<int>(state.get_slave_uuids().size());
            int meteos = static_cast<int>(state.get_meteo_uuids().size());

            std::string ss_slaves = "Slaves: " + std::to_string(slaves);
            std::string ss_meteos = "Meteos: " + std::to_string(meteos);

            float temp = 0.0f, hum = 0.0f;
            state.get_sensor_data(temp, hum);

            std::ostringstream ss_sensor;
            if (meteos > 0 && (temp != 0.0f || hum != 0.0f)) {
                ss_sensor << "T:" << std::fixed << std::setprecision(1) << temp
                          << "C H:" << std::fixed << std::setprecision(1) << hum << "%";
            } else {
                ss_sensor << "Temp: N/A | Hum: N/A";
            }

            set_rows("** MASTER NODE **",
                     "--------------------",
                     wifi_str.c_str(),
                     mqtt_str.c_str(),
                     ss_slaves.c_str(),
                     ss_meteos.c_str(),
                     ss_sensor.str().c_str());
            break;
        }

        // ── Screen 1: SLAVES ────────────────────────────────────────────────
        case ScreenID::SLAVES: {
            auto uuids = state.get_slave_uuids();
            int total = static_cast<int>(uuids.size());

            // Clamp scroll offset
            int max_offset = (total > 5) ? (total - 5) : 0;
            if (list_scroll_offset > max_offset) list_scroll_offset = max_offset;

            std::string header = "SLAVES (" + std::to_string(total) + ")";
            std::string rows[5];
            for (int i = 0; i < 5; i++) {
                int idx = list_scroll_offset + i;
                if (idx < total) {
                    // Truncate UUID to 20 chars to fit display
                    rows[i] = uuids[idx].substr(0, 20);
                } else {
                    rows[i] = "";
                }
            }

            set_rows(header.c_str(),
                     "--------------------",
                     rows[0].c_str(),
                     rows[1].c_str(),
                     rows[2].c_str(),
                     rows[3].c_str(),
                     rows[4].c_str());
            break;
        }

        // ── Screen 2: METEOS ────────────────────────────────────────────────
        case ScreenID::METEOS: {
            auto uuids = state.get_meteo_uuids();
            int total = static_cast<int>(uuids.size());

            int max_offset = (total > 5) ? (total - 5) : 0;
            if (list_scroll_offset > max_offset) list_scroll_offset = max_offset;

            std::string header = "METEOS (" + std::to_string(total) + ")";
            std::string rows[5];
            for (int i = 0; i < 5; i++) {
                int idx = list_scroll_offset + i;
                if (idx < total) {
                    rows[i] = uuids[idx].substr(0, 20);
                } else {
                    rows[i] = "";
                }
            }

            set_rows(header.c_str(),
                     "--------------------",
                     rows[0].c_str(),
                     rows[1].c_str(),
                     rows[2].c_str(),
                     rows[3].c_str(),
                     rows[4].c_str());
            break;
        }

        // ── Screen 3: SENSOR ────────────────────────────────────────────────
        case ScreenID::SENSOR: {
            float temp = 0.0f, hum = 0.0f;
            state.get_sensor_data(temp, hum);

            std::ostringstream ss_temp, ss_hum;
            ss_temp << "Temp: " << std::fixed << std::setprecision(2) << temp << " C";
            ss_hum  << "Hum:  " << std::fixed << std::setprecision(2) << hum  << " %";

            int meteos = static_cast<int>(state.get_meteo_uuids().size());
            std::string src = "Sources: " + std::to_string(meteos) + " meteo(s)";

            set_rows("--- SENSOR DATA ---",
                     "--------------------",
                     ss_temp.str().c_str(),
                     ss_hum.str().c_str(),
                     src.c_str(),
                     "",
                     "");
            break;
        }

        // ── Screen 4: SYSTEM ────────────────────────────────────────────────
        case ScreenID::SYSTEM: {
            std::string uuid  = state.get_node_uuid();
            std::string mode  = "Mode: " + state.get_node_mode_str();
            std::string room  = "Room: " + state.get_room();

            // UUID may be long — split into two lines of 20 chars each
            std::string uuid1 = uuid.substr(0, 20);
            std::string uuid2 = (uuid.size() > 20) ? uuid.substr(20, 20) : "";

            set_rows("--- SYSTEM INFO ---",
                     "--------------------",
                     uuid1.c_str(),
                     uuid2.c_str(),
                     mode.c_str(),
                     room.c_str(),
                     "");
            break;
        }

        // ── Screen BOOTING: boot splash ───────────────────────────────────────
        case ScreenID::BOOTING: {
            set_rows("*** MASTER NODE ***",
                     "--------------------",
                     "",
                     "  Booting up...",
                     "",   // row 4 — spinner task writes here
                     "",
                     "");
            break;
        }

        // ── Screen SHUTDOWN: shutdown splash ──────────────────────────────────
        case ScreenID::SHUTDOWN: {
            set_rows("*** MASTER NODE ***",
                     "--------------------",
                     "",
                     " Shutting down...",
                     "",   // row 4 — spinner task writes here
                     "",
                     "");
            break;
        }

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  display_update_task
//  - Drives the spinner animation on BOOTING / SHUTDOWN screens
//  - Drains the button event queue and delegates to navigate()
//  - Refreshes live STATUS data every second
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::display_update_task(void* pvParameters) {
    auto& dm = DisplayManager::get_instance();
    QueueHandle_t btn_queue = ButtonManager::get_instance().get_event_queue();

    // ASCII spinner frames — cycles every 200 ms tick → ~2.5 rpm visual
    static constexpr const char* SPINNER_FRAMES[] = {
        "        |",
        "        /",
        "        -",
        "        \\"
    };
    static constexpr int SPINNER_COUNT = 4;

    while (true) {
        if (!dm.initialized) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Animate spinner on boot / shutdown splash screens
        if (dm.current_screen == ScreenID::BOOTING ||
            dm.current_screen == ScreenID::SHUTDOWN)
        {
            dm.spinner_frame = (dm.spinner_frame + 1) % SPINNER_COUNT;
            const char* frame = SPINNER_FRAMES[dm.spinner_frame];

            dm.lock();
            lv_label_set_text(dm.label_row[4], frame);
            lv_obj_invalidate(lv_display_get_screen_active(dm.display));
            dm.unlock();

            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Process all pending button events (non-blocking drain)
        ButtonEvent ev;
        while (btn_queue != nullptr &&
               xQueueReceive(btn_queue, &ev, 0) == pdTRUE)
        {
            dm.navigate(ev);
        }

        // Live refresh for STATUS screen (data changes every second)
        if (dm.current_screen == ScreenID::STATUS) {
            dm.render_screen(ScreenID::STATUS);
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // 5 Hz polling — snappy UI response
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  LVGL timer / flush tasks and callbacks (unchanged logic)
// ─────────────────────────────────────────────────────────────────────────────
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

void DisplayManager::example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(5); // 5ms tick
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
        lv_display_flush_ready(disp);
    }
}

bool DisplayManager::example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel,
                                                      esp_lcd_panel_io_event_data_t *edata,
                                                      void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}
