#pragma once

#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "button_manager.h"
#include <mutex>

// ── Screen identifiers ────────────────────────────────────────────────────────
enum class ScreenID : uint8_t {
    STATUS   = 0,  ///< WiFi / MQTT / Slaves / Meteos / Sensor summary
    SLAVES   = 1,  ///< Scrollable list of registered slave UUIDs
    METEOS   = 2,  ///< Scrollable list of registered meteo UUIDs
    SENSOR   = 3,  ///< Full sensor reading (temperature + humidity)
    SYSTEM   = 4,  ///< Node UUID, Mode, Room
    _COUNT   = 5,  ///< Number of navigable screens
    BOOTING  = 5,  ///< Transient boot splash (not in navigation cycle)
    SHUTDOWN = 6   ///< Transient shutdown splash (not in navigation cycle)
};

static constexpr int SCREEN_COUNT = static_cast<int>(ScreenID::_COUNT);

class DisplayManager {
public:
    static DisplayManager& get_instance();

    // Prevent copying
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    void init(gpio_num_t sda_pin, gpio_num_t scl_pin);
    void start();

    /// Show a full-screen "Booting up..." splash immediately (blocks until rendered)
    void show_boot_screen();

    /// Show a full-screen "Shutting down..." splash and block briefly so it is visible
    void show_shutdown_screen();

    /// Transition from BOOTING splash to the normal STATUS screen (call once boot is complete)
    void transition_to_status();

    void lock();
    void unlock();

    /// Handle a navigation button event (called from display_update_task)
    void navigate(ButtonEvent event);

private:
    DisplayManager() = default;
    ~DisplayManager() = default;

    static void display_update_task(void* pvParameters);
    static void lvgl_port_task(void* pvParameters);
    static void screen_invalidate_task(void* pvParameters);
    static void example_increase_lvgl_tick(void *arg);

    static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
    static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

    /// Re-render the 7 label slots for the given screen
    void render_screen(ScreenID screen);

    bool initialized{false};
    esp_lcd_panel_handle_t panel_handle{nullptr};
    esp_lcd_panel_io_handle_t io_handle{nullptr};
    lv_display_t *display{nullptr};
    void *draw_buf{nullptr};
    std::mutex lvgl_mutex;

    // ── Navigation state ─────────────────────────────────────────────────────
    ScreenID current_screen{ScreenID::STATUS};
    int list_scroll_offset{0};  ///< Scroll offset for list screens (SLAVES / METEOS)
    int spinner_frame{0};       ///< Frame counter for the boot/shutdown spinner animation

    // ── 7 re-used label slots (row 0–6) ──────────────────────────────────────
    lv_obj_t *label_row[7]{};
};
