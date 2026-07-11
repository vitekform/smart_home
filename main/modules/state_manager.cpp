#include "state_manager.h"
#include <algorithm>
#include <iostream>

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

std::string SystemStateManager::get_node_uuid() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return node_uuid;
}

void SystemStateManager::set_node_uuid(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(state_mutex);
    node_uuid = uuid;
}

std::string SystemStateManager::get_room() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return node_room;
}

void SystemStateManager::set_room(const std::string& room) {
    std::lock_guard<std::mutex> lock(state_mutex);
    node_room = room;
}

std::string SystemStateManager::get_tasks_string() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return tasks_string;
}

void SystemStateManager::set_tasks_string(const std::string& tasks_str) {
    std::lock_guard<std::mutex> lock(state_mutex);
    tasks_string = tasks_str;
}

std::string SystemStateManager::get_node_mode_str() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    switch (current_node_mode) {
        case NodeMode::MASTER:
            return "MASTER";
        case NodeMode::SLAVE:
            return "SLAVE";
        case NodeMode::METEO:
            return "METEO";
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

void SystemStateManager::add_slave_uuid(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = std::find_if(slave_registry.begin(), slave_registry.end(),
                           [&uuid](const SlaveInfo& info) { return info.uuid == uuid; });
    if (it == slave_registry.end()) {
        slave_registry.push_back({uuid, 0, true});
    } else {
        it->responded_this_round = true;
        it->missed_count = 0;
    }
}

void SystemStateManager::add_meteo_uuid(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = std::find_if(meteo_registry.begin(), meteo_registry.end(),
                           [&uuid](const MeteoInfo& info) { return info.uuid == uuid; });
    if (it == meteo_registry.end()) {
        meteo_registry.push_back({uuid, 0, true});
    } else {
        it->responded_this_round = true;
        it->missed_count = 0;
    }
}

bool SystemStateManager::has_slave_uuid(const std::string& uuid) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = std::find_if(slave_registry.begin(), slave_registry.end(),
                           [&uuid](const SlaveInfo& info) { return info.uuid == uuid; });
    return it != slave_registry.end();
}

bool SystemStateManager::has_meteo_uuid(const std::string& uuid) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = std::find_if(meteo_registry.begin(), meteo_registry.end(),
                           [&uuid](const MeteoInfo& info) { return info.uuid == uuid; });
    return it != meteo_registry.end();
}

std::vector<std::string> SystemStateManager::get_slave_uuids() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::vector<std::string> uuids;
    uuids.reserve(slave_registry.size());
    for (const auto& info : slave_registry) {
        uuids.push_back(info.uuid);
    }
    return uuids;
}

std::vector<std::string> SystemStateManager::get_meteo_uuids() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::vector<std::string> uuids;
    uuids.reserve(meteo_registry.size());
    for (const auto& info : meteo_registry) {
        uuids.push_back(info.uuid);
    }
    return uuids;
}

void SystemStateManager::clear_slave_uuids() {
    std::lock_guard<std::mutex> lock(state_mutex);
    slave_registry.clear();
}

void SystemStateManager::clear_meteo_uuids() {
    std::lock_guard<std::mutex> lock(state_mutex);
    meteo_registry.clear();
}

void SystemStateManager::process_round_timeout() {
    std::lock_guard<std::mutex> lock(state_mutex);
    for (auto it = slave_registry.begin(); it != slave_registry.end(); ) {
        if (!it->responded_this_round) {
            it->missed_count++;
            if (it->missed_count >= 2) {
                std::cout << "[MASTER] Slave " << it->uuid << " failed to respond 2 times in a row. Removing from registry." << std::endl;
                it = slave_registry.erase(it);
                continue;
            }
        } else {
            // Reset for next round
            it->responded_this_round = false;
        }
        ++it;
    }
    // Meteo
    for (auto it = meteo_registry.begin(); it != meteo_registry.end(); ) {
        if (!it->responded_this_round) {
            it->missed_count++;
            if (it->missed_count >= 2) {
                std::cout << "[MASTER] Meteo " << it->uuid << " failed to respond 2 times in a row. Removing from registry." << std::endl;
                it = meteo_registry.erase(it);
                continue;
            }
        } else {
            // Reset for next round
            it->responded_this_round = false;
        }
        ++it;
    }
}
