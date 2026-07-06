#include "mqtt_manager.h"
#include "state_manager.h"
#include "esp_log.h"
#include <vector>

static const char *TAG = "MQTT_MANAGER";

MqttManager* MqttManager::s_instance = nullptr;

MqttManager::MqttManager() : client(nullptr) {
    s_instance = this;
}

MqttManager::~MqttManager() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }
}

void MqttManager::init(const std::string& broker_url, const std::string& username, const std::string& password, const std::string& root_ca) {
    this->broker_url = broker_url;
    this->username = username;
    this->password = password;
    this->root_ca = root_ca;
    
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = this->broker_url.c_str();
    mqtt_cfg.credentials.username = this->username.c_str();
    mqtt_cfg.credentials.authentication.password = this->password.c_str();
    mqtt_cfg.broker.address.port = 8883;
    mqtt_cfg.broker.verification.certificate = this->root_ca.c_str();

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, this);
    esp_mqtt_client_start(client);
}

void MqttManager::set_subscription_topic(const std::string& topic) {
    this->subscription_topic = topic;
}

void MqttManager::set_command_callback(CommandCallback callback) {
    command_callback = callback;
}

void MqttManager::broadcast(const std::string& channel, const std::string& message) {
    if (s_instance && s_instance->client) {
        int msg_id = esp_mqtt_client_publish(s_instance->client, channel.c_str(), message.c_str(), message.length(), 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    } else {
        ESP_LOGW(TAG, "MQTT client/instance not initialized, cannot broadcast");
    }
}

void MqttManager::mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    MqttManager* manager = static_cast<MqttManager*>(handler_args);

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            SystemStateManager::get_instance().set_mqtt_connected(true);
            if (!manager->subscription_topic.empty()) {
                int msg_id = esp_mqtt_client_subscribe(manager->client, manager->subscription_topic.c_str(), 0);
                ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            SystemStateManager::get_instance().set_mqtt_connected(false);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            if (manager->command_callback) {
                std::string topic(event->topic, event->topic_len);
                std::string data(event->data, event->data_len);
                manager->command_callback(topic, data);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}
