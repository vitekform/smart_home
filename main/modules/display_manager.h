#pragma once

#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include <mutex>

class DisplayManager {
public:
    static DisplayManager& get_instance();

    // Prevent copying
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    void init(gpio_num_t sda_pin, gpio_num_t scl_pin);
    void start();

    void lock();
    void unlock();

private:
    DisplayManager() = default;
    ~DisplayManager() = default;

    static void display_update_task(void* pvParameters);
    static void lvgl_port_task(void* pvParameters);
    static void screen_invalidate_task(void* pvParameters);
    static void example_increase_lvgl_tick(void *arg);
    
    static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
    static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

    bool initialized{false};
    esp_lcd_panel_handle_t panel_handle{nullptr};
    esp_lcd_panel_io_handle_t io_handle{nullptr};
    lv_display_t *display{nullptr};
    void *draw_buf{nullptr};
    std::mutex lvgl_mutex;

    // UI labels
    lv_obj_t *label_header{nullptr};
    lv_obj_t *label_divider{nullptr};
    lv_obj_t *label_wifi{nullptr};
    lv_obj_t *label_mqtt{nullptr};
    lv_obj_t *label_slaves{nullptr};
    lv_obj_t *label_meteos{nullptr};
    lv_obj_t *label_sensor{nullptr};
};
