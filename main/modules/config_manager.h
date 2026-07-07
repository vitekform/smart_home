#pragma once

#include <string>
#include "enums/NodeMode.h"


struct AppConfig {
    std::string wifi_ssid;
    std::string wifi_pass;
    int wifi_retry;
    std::string mqtt_broker_url;
    std::string mqtt_command_topic;
    std::string mqtt_internal_topic;
    std::string mqtt_client_id;
    std::string mqtt_pass;
    NodeMode node_mode;
    std::string node_uuid;
};

class ConfigManager {
public:
    /**
     * @brief Load configuration from config.json. If the file does not exist,
     * it creates a default configuration file and saves it.
     * 
     * @param config The AppConfig struct to populate.
     * @param path Path to the config file (default: "/config.json").
     * @return true On success.
     * @return false On failure.
     */
    static bool load(AppConfig& config, const std::string& path = "/config.json");

    /**
     * @brief Save the configuration to a file in JSON format.
     * 
     * @param config The AppConfig struct containing the settings to save.
     * @param path Path to save the config file (default: "/config.json").
     * @return true On success.
     * @return false On failure.
     */
    static bool save(const AppConfig& config, const std::string& path = "/config.json");

    /**
     * @brief Load default configurations into the struct.
     * 
     * @param config The AppConfig struct to write default configurations to.
     */
    static void get_default(AppConfig& config);
};
