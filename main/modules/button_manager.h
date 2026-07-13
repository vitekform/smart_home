#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// GPIO pin assignments for the 6 navigation buttons
#define BTN_PIN_UP     GPIO_NUM_16
#define BTN_PIN_LEFT   GPIO_NUM_17
#define BTN_PIN_SELECT GPIO_NUM_18
#define BTN_PIN_RIGHT  GPIO_NUM_19
#define BTN_PIN_DOWN   GPIO_NUM_23
#define BTN_PIN_BACK   GPIO_NUM_25

// Debounce threshold in milliseconds
#define BTN_DEBOUNCE_MS 50

enum class ButtonEvent : uint8_t {
    UP = 0,
    LEFT,
    SELECT,
    RIGHT,
    DOWN,
    BACK
};

class ButtonManager {
public:
    static ButtonManager& get_instance();

    // Prevent copying
    ButtonManager(const ButtonManager&) = delete;
    ButtonManager& operator=(const ButtonManager&) = delete;

    /**
     * Configure all button GPIOs with edge-triggered ISRs.
     * External 10kΩ pull-ups are used; buttons are active-LOW.
     * No internal pull-ups are enabled.
     */
    void init();

    /**
     * Returns the FreeRTOS queue handle that consumers (e.g. DisplayManager)
     * should use to receive ButtonEvent values.
     */
    QueueHandle_t get_event_queue() const;

private:
    ButtonManager() = default;
    ~ButtonManager() = default;

    static void IRAM_ATTR gpio_isr_handler(void* arg);

    QueueHandle_t event_queue{nullptr};
    bool initialized{false};
};
