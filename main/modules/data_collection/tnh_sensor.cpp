#include "tnh_sensor.h"
#include "modules/state_manager.h"
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modules/mqtt_manager.h"

TnHSensor::TnHSensor(gpio_num_t pin) : gpio_pin(pin) {}

void TnHSensor::start() {
    if (SystemStateManager::get_instance().get_node_mode() == NodeMode::MASTER)
    {
        xTaskCreate(sensor_task, "tnh_sensor_task", 4096, this, 5, nullptr);
    }
}

void TnHSensor::sensor_task(void* pvParameters) {
    auto* self = static_cast<TnHSensor*>(pvParameters);
    float humidity = 0.0f;
    float temperature = 0.0f;
    bool connected = true;
    uint8_t retry_count = 0;

    std::cout << "[DHT11] Starting DHT11 reading task on GPIO " << self->gpio_pin << "..." << std::endl;

    while (true) {
        if (connected) {
            if (dht_read_float_data(DHT_TYPE_DHT11, self->gpio_pin, &humidity, &temperature) == ESP_OK) {
                std::cout << "[DHT11] Temperature: " << temperature << " °C, Humidity: " << humidity << " %" << std::endl;
                SystemStateManager::get_instance().set_sensor_data(temperature, humidity);
                std::string msg = "";
                msg.append("tmp:");
                msg.append(std::to_string(temperature));
                msg.append(" hum:");
                msg.append(std::to_string(humidity));
                msg.append("\\END\\");
                MqttManager::broadcast("smarthome/internal", msg);
            } else {
                retry_count++;
                std::cerr << "[DHT11] Failed to read data from sensor on GPIO " << self->gpio_pin << std::endl;
                if (retry_count >= 5) {
                    connected = false;
                    retry_count = 0;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else {
            std::cout << "[DHT11] Waiting for connection..." << std::endl;
            vTaskDelay(pdMS_TO_TICKS(300*1000));
            connected = true;
            std::cout << "[DHT11] Attempting connection...";
        }
    }
}
