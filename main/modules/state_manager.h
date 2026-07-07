#pragma once

#include "enums/NodeMode.h"
#include <mutex>
#include <string>
#include <vector>

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

    // Node UUID
    std::string get_node_uuid() const;
    void set_node_uuid(const std::string& uuid);

    // WiFi connection state
    bool is_wifi_connected() const;
    void set_wifi_connected(bool connected);

    // MQTT connection state
    bool is_mqtt_connected() const;
    void set_mqtt_connected(bool connected);

    // Sensor readings
    void set_sensor_data(float temp, float hum);
    void get_sensor_data(float& temp, float& hum) const;

    // Slave Registry
    void add_slave_uuid(const std::string& uuid);
    bool has_slave_uuid(const std::string& uuid) const;
    std::vector<std::string> get_slave_uuids() const;
    void clear_slave_uuids();
    void process_round_timeout();

    // Meteo Registry
    void add_meteo_uuid(const std::string& uuid);
    bool has_meteo_uuid(const std::string& uuid) const;
    std::vector<std::string> get_meteo_uuids() const;
    void clear_meteo_uuids();

private:
    SystemStateManager() = default;
    ~SystemStateManager() = default;

    struct SlaveInfo {
        std::string uuid;
        int missed_count = 0;
        bool responded_this_round = false;
    };

    struct MeteoInfo
    {
        std::string uuid;
        int missed_count = 0;
        bool responded_this_round = false;
    };

    mutable std::mutex state_mutex;
    NodeMode current_node_mode{NodeMode::INACTIVE};
    std::string node_uuid;
    bool wifi_connected{false};
    bool mqtt_connected{false};
    float last_temperature{0.0f};
    float last_humidity{0.0f};
    std::vector<SlaveInfo> slave_registry;
    std::vector<MeteoInfo> meteo_registry;
};
