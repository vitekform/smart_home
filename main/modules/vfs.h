#pragma once

#include <string>
#include <vector>
#include "esp_err.h"

class VFS {
public:
    /**
     * @brief Initialize and mount the LittleFS partition.
     * 
     * @param partition_label The label of the partition in the partition table (default: "storage").
     * @param base_path The mount point in the virtual file system (default: "/littlefs").
     * @return esp_err_t ESP_OK on success, or an error code.
     */
    static esp_err_t init(const char* partition_label = "storage", const char* base_path = "/littlefs");

    /**
     * @brief Deinitialize and unmount the LittleFS partition.
     * 
     * @param partition_label The label of the partition to unmount.
     * @return esp_err_t ESP_OK on success, or an error code.
     */
    static esp_err_t deinit(const char* partition_label = "storage");

    /**
     * @brief Check if a file or directory exists.
     * 
     * @param path The relative path from the base path (e.g., "/config.json").
     * @return true If the file exists.
     * @return false If the file does not exist.
     */
    static bool exists(const std::string& path);

    /**
     * @brief Read the entire contents of a file into a string.
     * 
     * @param path The relative path from the base path.
     * @param content Output string to store the file contents.
     * @return true On success.
     * @return false On failure.
     */
    static bool read_file(const std::string& path, std::string& content);

    /**
     * @brief Write content to a file. Overwrites the file if it already exists, or creates it.
     * 
     * @param path The relative path from the base path.
     * @param content The string content to write.
     * @return true On success.
     * @return false On failure.
     */
    static bool write_file(const std::string& path, const std::string& content);

    /**
     * @brief Append content to a file. Creates the file if it does not exist.
     * 
     * @param path The relative path from the base path.
     * @param content The string content to append.
     * @return true On success.
     * @return false On failure.
     */
    static bool append_file(const std::string& path, const std::string& content);

    /**
     * @brief Delete a file.
     * 
     * @param path The relative path from the base path.
     * @return true On success.
     * @return false On failure.
     */
    static bool delete_file(const std::string& path);

    /**
     * @brief Search and replace all occurrences of a target string in a file.
     * 
     * @param path The relative path from the base path.
     * @param target The string to search for.
     * @param replacement The string to replace it with.
     * @return true On success.
     * @return false On failure.
     */
    static bool replace_in_file(const std::string& path, const std::string& target, const std::string& replacement);

    /**
     * @brief Get the size of a file in bytes.
     * 
     * @param path The relative path from the base path.
     * @return long The size of the file in bytes, or -1 if the file does not exist or cannot be accessed.
     */
    static long get_file_size(const std::string& path);

    /**
     * @brief List files in a directory.
     * 
     * @param path The relative path from the base path (e.g., "/" or "/subdir").
     * @param files Output vector to store file names.
     * @return true On success.
     * @return false On failure.
     */
    static bool list_dir(const std::string& path, std::vector<std::string>& files);

    /**
     * @brief Check if VFS is mounted.
     */
    static bool is_mounted() { return s_mounted; }

private:
    static std::string s_base_path;
    static bool s_mounted;

    /**
     * @brief Helper to prepend the base mount path to a relative path.
     */
    static std::string prepend_base_path(const std::string& path);
};
