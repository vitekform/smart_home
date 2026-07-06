#pragma once

#include "driver/gpio.h"
#include "dht.h"

class TnHSensor {
public:
    explicit TnHSensor(gpio_num_t pin);
    void start();

private:
    static void sensor_task(void* pvParameters);
    gpio_num_t gpio_pin;
};
