#pragma once

#include "enums/NodeMode.h"
#include <mutex>
#include <string>

class SystemStateManager {
public:
    static SystemStateManager& get_instance();

    // Prevent copying
    SystemStateManager(const SystemStateManager&) = delete;
    SystemStateManager& operator=(const SystemStateManager&) = delete;

    // Node Mode (State)
    NodeMode get_node_mode() const;
    void set_node_mode(NodeMode mode);
    std::string get_node_mode_str() const;

    // WiFi connection state
    bool is_wifi_connected() const;
    void set_wifi_connected(bool connected);

    // MQTT connection state
    bool is_mqtt_connected() const;
    void set_mqtt_connected(bool connected);

    // Sensor readings
    void set_sensor_data(float temp, float hum);
    void get_sensor_data(float& temp, float& hum) const;

private:
    SystemStateManager() = default;
    ~SystemStateManager() = default;

    mutable std::mutex state_mutex;
    NodeMode current_node_mode{NodeMode::INACTIVE};
    bool wifi_connected{false};
    bool mqtt_connected{false};
    float last_temperature{0.0f};
    float last_humidity{0.0f};
};
