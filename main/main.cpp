#include <iostream>
#include <cstring>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "modules/fastfetch.h"

// Configuration Target
#define WIFI_SSID      "wifič-formánci"
#define WIFI_PASS      "8!!mU%QW09wk*AT7c8G%"
#define MAXIMUM_RETRY  5

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WiFi_CPP";
static int s_retry_num = 0;

// C-compatible event handler callback wrapper
extern "C" {
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_retry_num < MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "Retrying connection to the AP...");
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGE(TAG, "Failed to connect to the AP");
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Allocated IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

class WirelessManager {
public:
    void init() {
        // 1. Initialize Non-Volatile Storage (NVS) - Required for Wi-Fi to store calibration data
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        s_wifi_event_group = xEventGroupCreate();

        // 2. Network Layer Initialization
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // 3. Register Event Handlers
        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            &instance_got_ip));

        // 4. Configure Wi-Fi Credentials safely inside standard raw primitives
        wifi_config_t wifi_config = {};
        std::strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID);
        std::strcpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASS);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "Wireless Manager Station initialized completely.");

        // 5. Block task until connection succeeds or fails completely
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            std::cout << "🌐 [C++] Network established successfully to SSID: " << WIFI_SSID << std::endl;
        } else if (bits & WIFI_FAIL_BIT) {
            std::cout << "❌ [C++] Failed to authenticate or reach SSID: " << WIFI_SSID << std::endl;
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }
};

// Main execution frame
extern "C" void app_main(void)
{
    runHwProbe();
    std::cout << "🚀 Starting Pure C++ Asynchronous System Framework..." << std::endl;
    
    WirelessManager wm;
    wm.init();

    int8_t ran_times = 0;

    while (true) {
        if (ran_times == 0)
        {
            std::cout << "Yo this is from main loop";
        }
        ran_times++;
        printf("Restarting in %ss\n", std::to_string(10-ran_times).c_str());
        if (ran_times == 10)
        {
            ran_times = 0;
            std::cout<< "Restarting";
            esp_restart();
        }
        if (ran_times > 10 || ran_times < 0)
        {
            std::cout << "Out of bounds. HOW THE FUCK DID WE GET THERE?";
            ran_times = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}