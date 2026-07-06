#include "state_manager.h"

SystemStateManager& SystemStateManager::get_instance() {
    static SystemStateManager instance;
    return instance;
}

NodeMode SystemStateManager::get_node_mode() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return current_node_mode;
}

void SystemStateManager::set_node_mode(NodeMode mode) {
    std::lock_guard<std::mutex> lock(state_mutex);
    current_node_mode = mode;
}

std::string SystemStateManager::get_node_mode_str() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    switch (current_node_mode) {
        case NodeMode::MASTER:
            return "MASTER";
        case NodeMode::SLAVE:
            return "SLAVE";
        case NodeMode::INACTIVE:
        default:
            return "INACTIVE";
    }
}

bool SystemStateManager::is_wifi_connected() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return wifi_connected;
}

void SystemStateManager::set_wifi_connected(bool connected) {
    std::lock_guard<std::mutex> lock(state_mutex);
    wifi_connected = connected;
}

bool SystemStateManager::is_mqtt_connected() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return mqtt_connected;
}

void SystemStateManager::set_mqtt_connected(bool connected) {
    std::lock_guard<std::mutex> lock(state_mutex);
    mqtt_connected = connected;
}

void SystemStateManager::set_sensor_data(float temp, float hum) {
    std::lock_guard<std::mutex> lock(state_mutex);
    last_temperature = temp;
    last_humidity = hum;
}

void SystemStateManager::get_sensor_data(float& temp, float& hum) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    temp = last_temperature;
    hum = last_humidity;
}
