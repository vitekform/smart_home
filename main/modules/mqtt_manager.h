#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "mqtt_client.h"
#include <string>
#include <functional>

class MqttManager {
public:
    using CommandCallback = std::function<void(const std::string& topic, const std::string& data)>;

    MqttManager();
    ~MqttManager();

    void init(const std::string& broker_url, const std::string& username, const std::string& password, const std::string& root_ca);
    void set_subscription_topic(const std::string& topic);
    void set_command_callback(CommandCallback callback);
    static void broadcast(const std::string& channel, const std::string& message);

private:
    static MqttManager* s_instance;
    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    
    esp_mqtt_client_handle_t client;
    CommandCallback command_callback;
    std::string broker_url;
    std::string subscription_topic;
    std::string username;
    std::string password;
    std::string root_ca;
};

#endif // MQTT_MANAGER_H
