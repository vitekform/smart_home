#include "modules/config_manager.h"
#include "modules/vfs.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"
#include <cstdlib>

static const char* TAG = "ConfigManager";

static std::string generate_uuid_v4() {
    uint8_t uuid[16];
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = esp_random();
        uuid[i] = r & 0xFF;
        uuid[i+1] = (r >> 8) & 0xFF;
        uuid[i+2] = (r >> 16) & 0xFF;
        uuid[i+3] = (r >> 24) & 0xFF;
    }
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3],
             uuid[4], uuid[5],
             uuid[6], uuid[7],
             uuid[8], uuid[9],
             uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return std::string(buf);
}

void ConfigManager::get_default(AppConfig& config) {
    config.wifi_ssid = "NSA Surveillance Van - 942";
    config.wifi_pass = "ganamaga";
    config.wifi_retry = 5;
    config.mqtt_broker_url = "mqtts://b4aab6512bbd4adc8bcf3981fe64f1dc.s1.eu.hivemq.cloud";
    config.mqtt_command_topic = "smarthome/admincmd";
    config.mqtt_internal_topic = "smarthome/internal";
    config.mqtt_client_id = "esp_main";
    config.mqtt_pass = "qGq5o11h16zVvcncTYhv";
    config.node_mode = NodeMode::INACTIVE;
    config.node_uuid = generate_uuid_v4();
}

bool ConfigManager::load(AppConfig& config, const std::string& path) {
    // Start with defaults to ensure all fields are initialized
    get_default(config);

    if (!VFS::exists(path)) {
        ESP_LOGI(TAG, "Config file %s does not exist, creating with defaults...", path.c_str());
        if (!save(config, path)) {
            ESP_LOGE(TAG, "Failed to create default config file");
            return false;
        }
        return true;
    }

    std::string content;
    if (!VFS::read_file(path, content)) {
        ESP_LOGE(TAG, "Failed to read config file: %s", path.c_str());
        return false;
    }

    cJSON* root = cJSON_Parse(content.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse JSON content from config file");
        return false;
    }

    bool needs_save = false;

    cJSON* wifi_ssid_item = cJSON_GetObjectItem(root, "wifi_ssid");
    if (cJSON_IsString(wifi_ssid_item) && wifi_ssid_item->valuestring != nullptr) {
        config.wifi_ssid = wifi_ssid_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* wifi_pass_item = cJSON_GetObjectItem(root, "wifi_pass");
    if (cJSON_IsString(wifi_pass_item) && wifi_pass_item->valuestring != nullptr) {
        config.wifi_pass = wifi_pass_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* wifi_retry_item = cJSON_GetObjectItem(root, "wifi_retry");
    if (cJSON_IsNumber(wifi_retry_item)) {
        config.wifi_retry = wifi_retry_item->valueint;
    } else {
        needs_save = true;
    }

    cJSON* mqtt_url_item = cJSON_GetObjectItem(root, "mqtt_broker_url");
    if (cJSON_IsString(mqtt_url_item) && mqtt_url_item->valuestring != nullptr) {
        config.mqtt_broker_url = mqtt_url_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* mqtt_topic_item = cJSON_GetObjectItem(root, "mqtt_command_topic");
    if (cJSON_IsString(mqtt_topic_item) && mqtt_topic_item->valuestring != nullptr) {
        config.mqtt_command_topic = mqtt_topic_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* mqtt_internal_topic_item = cJSON_GetObjectItem(root, "mqtt_internal_topic");
    if (cJSON_IsString(mqtt_internal_topic_item) && mqtt_internal_topic_item->valuestring != nullptr) {
        config.mqtt_internal_topic = mqtt_internal_topic_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* mqtt_client_item = cJSON_GetObjectItem(root, "mqtt_client_id");
    if (cJSON_IsString(mqtt_client_item) && mqtt_client_item->valuestring != nullptr) {
        config.mqtt_client_id = mqtt_client_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* mqtt_pass_item = cJSON_GetObjectItem(root, "mqtt_pass");
    if (cJSON_IsString(mqtt_pass_item) && mqtt_pass_item->valuestring != nullptr) {
        config.mqtt_pass = mqtt_pass_item->valuestring;
    } else {
        needs_save = true;
    }

    cJSON* node_mode = cJSON_GetObjectItem(root, "node_mode");
    if (cJSON_IsNumber(node_mode)) {
        config.node_mode = static_cast<NodeMode>(node_mode->valueint);
    } else {
        needs_save = true;
    }

    cJSON* node_uuid = cJSON_GetObjectItem(root, "node_uuid");
    if (cJSON_IsString(node_uuid) && node_uuid->valuestring != nullptr && strlen(node_uuid->valuestring) > 0) {
        config.node_uuid = node_uuid->valuestring;
    } else {
        // If node_uuid is missing or empty, generate a new one
        config.node_uuid = generate_uuid_v4();
        needs_save = true;
    }

    cJSON_Delete(root);

    if (needs_save) {
        ESP_LOGI(TAG, "Config schema updated or missing keys found. Saving updated config...");
        save(config, path);
    }

    ESP_LOGI(TAG, "Config loaded successfully from %s", path.c_str());
    return true;
}

bool ConfigManager::save(const AppConfig& config, const std::string& path) {
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to create cJSON object");
        return false;
    }

    cJSON_AddStringToObject(root, "wifi_ssid", config.wifi_ssid.c_str());
    cJSON_AddStringToObject(root, "wifi_pass", config.wifi_pass.c_str());
    cJSON_AddNumberToObject(root, "wifi_retry", config.wifi_retry);
    cJSON_AddStringToObject(root, "mqtt_broker_url", config.mqtt_broker_url.c_str());
    cJSON_AddStringToObject(root, "mqtt_command_topic", config.mqtt_command_topic.c_str());
    cJSON_AddStringToObject(root, "mqtt_internal_topic", config.mqtt_internal_topic.c_str());
    cJSON_AddStringToObject(root, "mqtt_client_id", config.mqtt_client_id.c_str());
    cJSON_AddStringToObject(root, "mqtt_pass", config.mqtt_pass.c_str());
    cJSON_AddNumberToObject(root, "node_mode", static_cast<int>(config.node_mode));
    cJSON_AddStringToObject(root, "node_uuid", config.node_uuid.c_str());

    char* rendered = cJSON_Print(root);
    cJSON_Delete(root);

    if (rendered == nullptr) {
        ESP_LOGE(TAG, "Failed to render config to JSON string");
        return false;
    }

    std::string json_str(rendered);
    std::free(rendered);

    if (!VFS::write_file(path, json_str)) {
        ESP_LOGE(TAG, "Failed to write JSON to config file: %s", path.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Config saved successfully to %s", path.c_str());
    return true;
}
