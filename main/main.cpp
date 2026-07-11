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
#include "modules/mqtt_manager.h"
#include "modules/vfs.h"
#include "modules/config_manager.h"
#include "modules/state_manager.h"
#include "modules/data_collection/tnh_sensor.h"
#include "modules/data_collection/out_sensor.h"
#include "modules/display_manager.h"
#include <vector>
#include <string_view>
#include <ranges>
#include "driver/gpio.h"

const char* x1root = "-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WiFi_CPP";
static int s_retry_num = 0;
static int s_max_retry = 5;

std::vector<std::string> splitBySequence(const std::string& text, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = text.find(delimiter);

    while (end != std::string::npos) {
        // Extrahujeme podřetězec od startu po nalezený oddělovač
        tokens.push_back(text.substr(start, end - start));
        // Posuneme start za nalezený oddělovač
        start = end + delimiter.length();
        // Hledáme další výskyt oddělovače
        end = text.find(delimiter, start);
    }

    // Přidáme poslední zbývající část řetězce
    tokens.push_back(text.substr(start));
    return tokens;
}

// C-compatible event handler callback wrapper
extern "C" {
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            SystemStateManager::get_instance().set_wifi_connected(false);
            if (s_retry_num < s_max_retry) {
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
            SystemStateManager::get_instance().set_wifi_connected(true);
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

class WirelessManager {
public:
    void init(const AppConfig& config) {
        s_max_retry = config.wifi_retry;

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
        std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), config.wifi_ssid.c_str(), sizeof(wifi_config.sta.ssid));
        std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), config.wifi_pass.c_str(), sizeof(wifi_config.sta.password));
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
            std::cout << "[C++] Network established successfully to SSID: " << config.wifi_ssid << std::endl;
        } else if (bits & WIFI_FAIL_BIT) {
            std::cout << "[C++] Failed to authenticate or reach SSID: " << config.wifi_ssid << std::endl;
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }

        std::cout << "Node data:";
        std::string data = "";
        data.append("UUID - ");
        data.append(config.node_uuid);
        data.append("\n");
        data.append("Node state - ");
        if (config.node_mode == NodeMode::INACTIVE)
        {
            data.append("INACTIVE");
        }
        else if (config.node_mode == NodeMode::MASTER)
        {
            data.append("MASTER");
        }
        else if (config.node_mode == NodeMode::METEO)
        {
            data.append("METEO");
        }
        else
        {
            data.append("SLAVE");
        }
        data.append("\n");
        std::cout << data;
    }
};

void handle_command_topic(const std::string& topic, const std::string& data, AppConfig& config) {
    std::cout << "[MQTT] Received command on topic [" << topic << "]: " << data << std::endl;
    if (data == "restart") {
        std::cout << "Restarting system as requested..." << std::endl;
        esp_restart();
    } else if (data == "reset" || data == "reset_config") {
        std::cout << "Resetting config.json to defaults..." << std::endl;
        AppConfig defaultConfig;
        ConfigManager::get_default(defaultConfig);
        if (ConfigManager::save(defaultConfig)) {
            std::cout << "Config reset successful! Restarting system..." << std::endl;
            esp_restart();
        } else {
            std::cerr << "Failed to reset config!" << std::endl;
        }
    } else if (data.starts_with("set_state"))
    {
        /*
         * Structure
         * set_state <node_uuid> <new_state_number>
         */
        std::string delimiter = " ";
        std::vector<std::string> arr = splitBySequence(data, delimiter);
        if (arr.size() >= 3) {
            std::string uuid = arr[1];
            if (config.node_uuid == uuid) {
                int state = std::stoi(arr[2]);
                if (state == 0)
                {
                    config.node_mode = NodeMode::INACTIVE;
                    config.tasks.clear();
                }
                else if (state == 1)
                {
                    config.node_mode = NodeMode::MASTER;
                    config.tasks.clear();
                }
                else if (state == 2)
                {
                    config.node_mode = NodeMode::SLAVE;
                    config.tasks.clear();
                    TaskConfig default_task;
                    default_task.type = "HAC";
                    default_task.room = config.room;
                    default_task.heating_pin = 12;
                    default_task.cooling_pin = 13;
                    config.tasks.push_back(default_task);
                }
                else if (state == 3)
                {
                    config.node_mode = NodeMode::METEO;
                    config.tasks.clear();
                    TaskConfig default_task;
                    default_task.type = "dht11";
                    default_task.room = config.room;
                    default_task.pin_num = 5;
                    config.tasks.push_back(default_task);
                }
                SystemStateManager::get_instance().set_node_mode(config.node_mode);
                ConfigManager::save(config);
                std::cout << "[MQTT] Node state updated to " << state << " and saved successfully." << std::endl;
                // call restart
                std::cout << "Restarting system because of configuration change of mode" << std::endl;
                esp_restart();
            }
        }
    } else if (data.starts_with("set_room")) {
        std::string delimiter = " ";
        std::vector<std::string> arr = splitBySequence(data, delimiter);
        if (arr.size() >= 3) {
            std::string uuid = arr[1];
            if (config.node_uuid == uuid) {
                std::string room = arr[2];
                config.room = room;
                ConfigManager::save(config);
                std::cout << "[MQTT] Room updated to " << room << " and saved successfully. Restarting..." << std::endl;
                esp_restart();
            }
        }
    } else if (data.starts_with("set_tasks")) {
        std::string delimiter = " ";
        std::vector<std::string> arr = splitBySequence(data, delimiter);
        if (arr.size() >= 3) {
            std::string uuid = arr[1];
            if (config.node_uuid == uuid) {
                std::string tasks_payload = arr[2];
                bool success = false;
                if (config.node_mode == NodeMode::METEO) {
                    config.tasks.clear();
                    std::vector<std::string> task_tokens = splitBySequence(tasks_payload, ",");
                    for (const auto& token : task_tokens) {
                        std::vector<std::string> parts = splitBySequence(token, ";");
                        if (parts.size() >= 2) {
                            TaskConfig tc;
                            tc.type = parts[0];
                            tc.room = config.room;
                            if (tc.type == "dht11") {
                                tc.pin_num = std::stoi(parts[1]);
                                config.tasks.push_back(tc);
                            } else if (tc.type == "out" && parts.size() >= 3) {
                                tc.sda_pin = std::stoi(parts[1]);
                                tc.scl_pin = std::stoi(parts[2]);
                                config.tasks.push_back(tc);
                            }
                        }
                    }
                    success = true;
                } else if (config.node_mode == NodeMode::SLAVE) {
                    config.tasks.clear();
                    std::vector<std::string> task_tokens = splitBySequence(tasks_payload, ";");
                    for (const auto& token : task_tokens) {
                        std::vector<std::string> parts = splitBySequence(token, ":");
                        if (parts.size() >= 3) {
                            TaskConfig tc;
                            tc.type = parts[0];
                            tc.room = parts[1];
                            std::vector<std::string> pins = splitBySequence(parts[2], ",");
                            if (tc.type == "HAC" && pins.size() >= 2) {
                                tc.heating_pin = std::stoi(pins[0]);
                                tc.cooling_pin = std::stoi(pins[1]);
                                config.tasks.push_back(tc);
                            } else if (pins.size() >= 1) {
                                tc.heating_pin = std::stoi(pins[0]);
                                config.tasks.push_back(tc);
                            }
                        }
                    }
                    success = true;
                }
                
                if (success) {
                    ConfigManager::save(config);
                    std::cout << "[MQTT] Tasks updated and saved. Restarting..." << std::endl;
                    esp_restart();
                }
            }
        }
    }
}

void handle_internal_topic(const std::string& topic, const std::string& data) {
    std::cout << "[MQTT] Received internal message on topic [" << topic << "]: " << data << std::endl;

    if (SystemStateManager::get_instance().get_node_mode() == NodeMode::INACTIVE)
    {
        return;
    }
    if (SystemStateManager::get_instance().get_node_mode() == NodeMode::MASTER)
    {
        if (data.starts_with("dev_poll_rsp"))
        {
            std::string delimiter = " ";
            std::vector<std::string> arr = splitBySequence(data, delimiter);
            if (arr.size() >= 3) {
                std::string registered_uuid = arr[1];
                std::string node_t = arr[2];
                if (node_t == "SLAVE")
                {
                    if (!SystemStateManager::get_instance().has_slave_uuid(registered_uuid)) {
                        SystemStateManager::get_instance().add_slave_uuid(registered_uuid);
                        std::cout << "[MASTER] Registered new slave with UUID: " << registered_uuid << std::endl;
                    }
                    else {
                        std::cout << "[MASTER] Slave with UUID: " << registered_uuid << " is already registered." << std::endl;
                    }
                }
                else if (node_t == "METEO")
                {
                    if (!SystemStateManager::get_instance().has_meteo_uuid(registered_uuid)) {
                        SystemStateManager::get_instance().add_meteo_uuid(registered_uuid);
                        std::cout << "[MASTER] Registered new meteo node with UUID: " << registered_uuid << std::endl;
                    }
                    else {
                        std::cout << "[MASTER] Meteo node with UUID: " << registered_uuid << " is already registered." << std::endl;
                    }
                }

            }
        }
        else if (data.starts_with("meteo_data"))
        {
            std::string delimiter = " ";
            std::vector<std::string> arr = splitBySequence(data, delimiter);
            if (arr.size() >= 4) {
                std::string uuid = arr[1];
                float temp = (float)std::strtof(arr[2].c_str(), nullptr);
                float hum = (float)std::strtof(arr[3].c_str(), nullptr);
                SystemStateManager::get_instance().set_sensor_data(temp, hum);
                if (!SystemStateManager::get_instance().has_meteo_uuid(uuid)) {
                    SystemStateManager::get_instance().add_meteo_uuid(uuid);
                    std::cout << "[MASTER] Registered new meteo node from meteo_data with UUID: " << uuid << std::endl;
                }
            }
        }
    }
    // Slave or Meteo
    else
    {
        if (data.starts_with("dev_poll") && !data.starts_with("dev_poll_rsp"))
        {
            std::string msg = "dev_poll_rsp";
            msg.append(" ");
            msg.append(SystemStateManager::get_instance().get_node_uuid());
            msg.append(" ");
            if (SystemStateManager::get_instance().get_node_mode() == NodeMode::METEO)
            {
                msg.append("METEO");
                msg.append(" ");
                msg.append(SystemStateManager::get_instance().get_room().empty() ? "living_room" : SystemStateManager::get_instance().get_room());
                msg.append(" ");
                msg.append(SystemStateManager::get_instance().get_tasks_string().empty() ? "none" : SystemStateManager::get_instance().get_tasks_string());
            }
            else
            {
                msg.append("SLAVE");
                msg.append(" ");
                msg.append(SystemStateManager::get_instance().get_tasks_string().empty() ? "none" : SystemStateManager::get_instance().get_tasks_string());
            }
            MqttManager::broadcast("smarthome/internal", msg);
        }
        else if (data.starts_with("dev_set_pin"))
        {
            std::string delimiter = " ";
            std::vector<std::string> arr = splitBySequence(data, delimiter);
            if (arr.size() >= 4)
            {
                std::string uuid = arr.at(1);
                std::string pin = arr.at(2);
                std::string value = arr.at(3); // HIGH or LOW
                int val = 0;
                if (value == "HIGH")
                {
                    val = 1;
                }

                if (uuid == SystemStateManager::get_instance().get_node_uuid())
                {
                    std::cout<<"Setting pin " << pin << " to " << value << std::endl;
                    auto pin_num = static_cast<gpio_num_t>(std::stoi(pin));
                    gpio_reset_pin(pin_num);
                    gpio_set_direction(pin_num, GPIO_MODE_OUTPUT);
                    gpio_set_level(pin_num, val);
                }
                return;
            }
        }
    }
}

static std::string s_internal_topic;

static void master_poll_task(void* pvParameters) {
    std::cout << "[MASTER POLL] Starting master polling task. Interval: 2 minutes." << std::endl;
    while (true) {
        // Wait 2 minutes (120 seconds)
        vTaskDelay(pdMS_TO_TICKS(120000));

        if (SystemStateManager::get_instance().get_node_mode() == NodeMode::MASTER) {
            // First, process the round timeout (marks missed responses, removes inactive slaves, resets flags)
            SystemStateManager::get_instance().process_round_timeout();

            // Then, send the new dev_poll broadcast
            std::cout << "[MASTER POLL] Broadcasting dev_poll on topic: " << s_internal_topic << std::endl;
            MqttManager::broadcast(s_internal_topic, "dev_poll");
        }
    }
}

// Main execution frame
extern "C" void app_main(void)
{
    runHwProbe();
    std::cout << "Starting Pure C++ Asynchronous System Framework..." << std::endl;

    // Initialize VFS (LittleFS)
    esp_err_t err = VFS::init();
    if (err != ESP_OK) {
        std::cerr << "Failed to initialize VFS: " << esp_err_to_name(err) << std::endl;
    }

    AppConfig config;
    if (!ConfigManager::load(config)) {
        std::cerr << "Failed to load/create config file! Using defaults." << std::endl;
        ConfigManager::get_default(config);
    }
    SystemStateManager::get_instance().set_node_mode(config.node_mode);
    SystemStateManager::get_instance().set_node_uuid(config.node_uuid);
    SystemStateManager::get_instance().set_room(config.room);

    // Build tasks string
    std::string tasks_str = "";
    if (config.node_mode == NodeMode::METEO) {
        for (const auto& task : config.tasks) {
            if (task.type == "dht11" && task.pin_num != -1) {
                tasks_str = "dht11;" + std::to_string(task.pin_num);
                break;
            } else if (task.type == "out" && task.sda_pin != -1 && task.scl_pin != -1) {
                tasks_str = "out;" + std::to_string(task.sda_pin) + ";" + std::to_string(task.scl_pin);
                break;
            }
        }
    } else if (config.node_mode == NodeMode::SLAVE) {
        for (size_t i = 0; i < config.tasks.size(); ++i) {
            const auto& task = config.tasks[i];
            tasks_str += task.type + ":" + task.room + ":";
            if (task.type == "HAC") {
                tasks_str += std::to_string(task.heating_pin) + "," + std::to_string(task.cooling_pin);
            } else {
                tasks_str += std::to_string(task.heating_pin);
            }
            if (i < config.tasks.size() - 1) {
                tasks_str += ";";
            }
        }
    }
    SystemStateManager::get_instance().set_tasks_string(tasks_str);
    
    WirelessManager wm;
    wm.init(config);

    // Create sensor read task based on dynamic configuration
    if (config.node_mode == NodeMode::METEO) {
        for (const auto& task : config.tasks) {
            if (task.type == "dht11" && task.pin_num != -1) {
                std::cout << "[METEO] Spawning DHT11 task on GPIO " << task.pin_num << std::endl;
                auto* dht = new TnHSensor(static_cast<gpio_num_t>(task.pin_num));
                dht->start();
                break;
            } else if (task.type == "out" && task.sda_pin != -1 && task.scl_pin != -1) {
                std::cout << "[METEO] Spawning Outdoor THP task on SDA: " << task.sda_pin << ", SCL: " << task.scl_pin << std::endl;
                auto* out = new OutSensor(static_cast<gpio_num_t>(task.sda_pin), static_cast<gpio_num_t>(task.scl_pin));
                out->start();
                break;
            }
        }
    }

    // Create master polling task if node is MASTER
    s_internal_topic = config.mqtt_internal_topic;
    if (config.node_mode == NodeMode::MASTER) {
        xTaskCreate(master_poll_task, "master_poll_task", 4096, nullptr, 5, nullptr);
#if CONFIG_IDF_TARGET_ESP32
        DisplayManager::get_instance().init(GPIO_NUM_21, GPIO_NUM_22);
#else
        DisplayManager::get_instance().init(GPIO_NUM_8, GPIO_NUM_9);
#endif
        DisplayManager::get_instance().start();
    }

    MqttManager mqtt;
    mqtt.set_topic1(config.mqtt_command_topic, [&config](const std::string& topic, const std::string& data) {
        handle_command_topic(topic, data, config);
    });
    mqtt.set_topic2(config.mqtt_internal_topic, [](const std::string& topic, const std::string& data) {
        handle_internal_topic(topic, data);
    });
    mqtt.init(config.mqtt_broker_url, config.mqtt_client_id, config.mqtt_pass, x1root);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}