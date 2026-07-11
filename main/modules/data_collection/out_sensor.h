#pragma once

#include "driver/gpio.h"

class OutSensor {
public:
    OutSensor(gpio_num_t sda, gpio_num_t scl);
    void start();

private:
    static void sensor_task(void* pvParameters);
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
};
