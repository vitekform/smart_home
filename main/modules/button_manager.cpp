#include "button_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <iostream>

static const char* TAG_BTN = "ButtonManager";

// Per-pin last-press timestamp for software debounce (microseconds)
static volatile int64_t s_last_press_us[6] = {0, 0, 0, 0, 0, 0};

// Mapping: gpio_num_t → {ButtonEvent, index into s_last_press_us}
struct BtnDesc {
    gpio_num_t  pin;
    ButtonEvent event;
    int         debounce_idx;
};

static const BtnDesc s_btn_map[] = {
    {BTN_PIN_UP,     ButtonEvent::UP,     0},
    {BTN_PIN_LEFT,   ButtonEvent::LEFT,   1},
    {BTN_PIN_SELECT, ButtonEvent::SELECT, 2},
    {BTN_PIN_RIGHT,  ButtonEvent::RIGHT,  3},
    {BTN_PIN_DOWN,   ButtonEvent::DOWN,   4},
    {BTN_PIN_BACK,   ButtonEvent::BACK,   5},
};
static constexpr int BTN_COUNT = sizeof(s_btn_map) / sizeof(s_btn_map[0]);

// Static queue handle used inside the ISR
static QueueHandle_t s_isr_queue = nullptr;

ButtonManager& ButtonManager::get_instance() {
    static ButtonManager instance;
    return instance;
}

// ISR called on any edge of any button pin.
// arg is cast to a pointer to the matching BtnDesc entry.
void IRAM_ATTR ButtonManager::gpio_isr_handler(void* arg) {
    const BtnDesc* btn = static_cast<const BtnDesc*>(arg);

    // Active-LOW: only act on the falling edge (pin goes LOW = button pressed)
    if (gpio_get_level(btn->pin) != 0) {
        return;
    }

    // Software debounce: ignore events that are too close together
    int64_t now_us = esp_timer_get_time();
    if ((now_us - s_last_press_us[btn->debounce_idx]) < (BTN_DEBOUNCE_MS * 1000LL)) {
        return;
    }
    s_last_press_us[btn->debounce_idx] = now_us;

    // Send event to queue (non-blocking, from ISR context)
    ButtonEvent ev = btn->event;
    BaseType_t higher_prio_task_woken = pdFALSE;
    xQueueSendFromISR(s_isr_queue, &ev, &higher_prio_task_woken);

    // Yield to a higher-priority task if one was woken
    if (higher_prio_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void ButtonManager::init() {
    if (initialized) {
        return;
    }

    // Create the event queue (depth 10 to absorb rapid presses)
    event_queue = xQueueCreate(10, sizeof(ButtonEvent));
    if (event_queue == nullptr) {
        std::cerr << "[ButtonManager] Failed to create event queue!" << std::endl;
        return;
    }
    s_isr_queue = event_queue;

    // Install the GPIO ISR service (shared across all pins)
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means it was already installed — that's fine
        std::cerr << "[ButtonManager] gpio_install_isr_service failed: " << esp_err_to_name(err) << std::endl;
        return;
    }

    // Configure each button GPIO and register its ISR
    for (int i = 0; i < BTN_COUNT; i++) {
        const BtnDesc& btn = s_btn_map[i];

        gpio_config_t cfg = {};
        cfg.pin_bit_mask = (1ULL << btn.pin);
        cfg.mode         = GPIO_MODE_INPUT;
        cfg.pull_up_en   = GPIO_PULLUP_DISABLE;   // External 10kΩ pull-ups
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type    = GPIO_INTR_ANYEDGE;      // Both edges; we filter in ISR

        err = gpio_config(&cfg);
        if (err != ESP_OK) {
            std::cerr << "[ButtonManager] gpio_config failed for GPIO" << btn.pin
                      << ": " << esp_err_to_name(err) << std::endl;
            continue;
        }

        // Pass the BtnDesc pointer as arg so the ISR knows which button fired
        err = gpio_isr_handler_add(btn.pin, gpio_isr_handler, (void*)&s_btn_map[i]);
        if (err != ESP_OK) {
            std::cerr << "[ButtonManager] gpio_isr_handler_add failed for GPIO" << btn.pin
                      << ": " << esp_err_to_name(err) << std::endl;
        }
    }

    initialized = true;
    std::cout << "[ButtonManager] Initialized. Monitoring "
              << BTN_COUNT << " buttons." << std::endl;
}

QueueHandle_t ButtonManager::get_event_queue() const {
    return event_queue;
}
