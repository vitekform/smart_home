#include "out_sensor.h"
#include "modules/state_manager.h"
#include "modules/mqtt_manager.h"
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <i2cdev.h>
#include <sht4x.h>
#include <bmp280.h>

OutSensor::OutSensor(gpio_num_t sda, gpio_num_t scl) : sda_pin(sda), scl_pin(scl) {}

void OutSensor::start() {
    if (SystemStateManager::get_instance().get_node_mode() == NodeMode::METEO)
    {
        xTaskCreate(sensor_task, "out_sensor_task", 4096, this, 5, nullptr);
    }
}

void OutSensor::sensor_task(void* pvParameters) {
    auto* self = static_cast<OutSensor*>(pvParameters);
    
    std::cout << "[OutSensor] Starting Outdoor Meteo task on SDA: " << self->sda_pin 
              << ", SCL: " << self->scl_pin << "..." << std::endl;

    // Initialize i2cdev library
    i2cdev_init();

    // SHT40 setup
    sht4x_t sht_dev = {};
    esp_err_t sht_err = sht4x_init_desc(&sht_dev, I2C_NUM_0, self->sda_pin, self->scl_pin);
    if (sht_err == ESP_OK) {
        sht_err = sht4x_init(&sht_dev);
    }
    if (sht_err != ESP_OK) {
        std::cerr << "[OutSensor] SHT40 initialization failed: " << esp_err_to_name(sht_err) << std::endl;
    }

    // BMP280 setup
    bmp280_t bmp_dev = {};
    bmp280_params_t bmp_params;
    bmp280_init_default_params(&bmp_params);
    
    // Try BMP280 address 0x77 first, then 0x76
    esp_err_t bmp_err = bmp280_init_desc(&bmp_dev, BMP280_I2C_ADDRESS_1, I2C_NUM_0, self->sda_pin, self->scl_pin);
    if (bmp_err == ESP_OK) {
        bmp_err = bmp280_init(&bmp_dev, &bmp_params);
        if (bmp_err != ESP_OK) {
            std::cout << "[OutSensor] BMP280 on 0x77 failed. Trying 0x76..." << std::endl;
            bmp280_init_desc(&bmp_dev, BMP280_I2C_ADDRESS_0, I2C_NUM_0, self->sda_pin, self->scl_pin);
            bmp_err = bmp280_init(&bmp_dev, &bmp_params);
        }
    }
    if (bmp_err != ESP_OK) {
        std::cerr << "[OutSensor] BMP280 initialization failed: " << esp_err_to_name(bmp_err) << std::endl;
    }

    while (true) {
        float sht_temp = 0.0f;
        float sht_hum = 0.0f;
        float bmp_temp = 0.0f;
        float bmp_pres = 0.0f;
        bool sht_ok = false;
        bool bmp_ok = false;

        if (sht_err == ESP_OK) {
            if (sht4x_measure(&sht_dev, &sht_temp, &sht_hum) == ESP_OK) {
                sht_ok = true;
            } else {
                std::cerr << "[OutSensor] Failed to read from SHT40" << std::endl;
            }
        }

        if (bmp_err == ESP_OK) {
            float dummy_hum;
            if (bmp280_read_float(&bmp_dev, &bmp_temp, &bmp_pres, &dummy_hum) == ESP_OK) {
                bmp_ok = true;
            } else {
                std::cerr << "[OutSensor] Failed to read from BMP280" << std::endl;
            }
        }

        if (sht_ok || bmp_ok) {
            float final_temp = 0.0f;
            float final_hum = 0.0f;

            if (sht_ok && bmp_ok) {
                final_temp = sht_temp;
                final_hum = sht_hum;
                std::cout << "[OutSensor] SHT40 Temp: " << sht_temp << " °C, Hum: " << sht_hum 
                          << " % | BMP280 Temp: " << bmp_temp << " °C, Pres: " << bmp_pres/100.0f << " hPa" << std::endl;
            } else if (sht_ok) {
                final_temp = sht_temp;
                final_hum = sht_hum;
                std::cout << "[OutSensor] SHT40 Temp: " << sht_temp << " °C, Hum: " << sht_hum << " % (BMP280 Offline)" << std::endl;
            } else {
                final_temp = bmp_temp;
                final_hum = 0.0f;
                std::cout << "[OutSensor] BMP280 Temp: " << bmp_temp << " °C, Pres: " << bmp_pres/100.0f << " hPa (SHT40 Offline)" << std::endl;
            }

            SystemStateManager::get_instance().set_sensor_data(final_temp, final_hum);
            
            std::string msg = "meteo_data";
            msg.append(" ");
            msg.append(SystemStateManager::get_instance().get_node_uuid());
            msg.append(" ");
            msg.append(std::to_string(final_temp));
            msg.append(" ");
            msg.append(std::to_string(final_hum));
            MqttManager::broadcast("smarthome/internal", msg);
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Read every 5 seconds
    }
}
