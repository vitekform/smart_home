#include "modules/vfs.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>

static const char* TAG = "VFS";

std::string VFS::s_base_path = "/littlefs";
bool VFS::s_mounted = false;

esp_err_t VFS::init(const char* partition_label, const char* base_path) {
    if (s_mounted) {
        ESP_LOGW(TAG, "VFS already mounted at %s", s_base_path.c_str());
        return ESP_OK;
    }
    s_base_path = base_path;

    ESP_LOGI(TAG, "Initializing LittleFS partition '%s' mounted at '%s'", partition_label, base_path);

    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = base_path;
    conf.partition_label = partition_label;
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d bytes, used: %d bytes", total, used);
    } else {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    }

    s_mounted = true;
    return ESP_OK;
}

esp_err_t VFS::deinit(const char* partition_label) {
    if (!s_mounted) {
        return ESP_OK;
    }
    esp_err_t ret = esp_vfs_littlefs_unregister(partition_label);
    if (ret == ESP_OK) {
        s_mounted = false;
        ESP_LOGI(TAG, "LittleFS unmounted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to unmount LittleFS (%s)", esp_err_to_name(ret));
    }
    return ret;
}

std::string VFS::prepend_base_path(const std::string& path) {
    if (path.empty()) {
        return s_base_path;
    }
    if (path[0] == '/') {
        return s_base_path + path;
    }
    return s_base_path + "/" + path;
}

bool VFS::exists(const std::string& path) {
    std::string full_path = prepend_base_path(path);
    struct stat st;
    return (stat(full_path.c_str(), &st) == 0);
}

bool VFS::read_file(const std::string& path, std::string& content) {
    std::string full_path = prepend_base_path(path);
    FILE* f = fopen(full_path.c_str(), "r");
    if (f == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path.c_str());
        return false;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return false;
    }

    content.resize(size);
    size_t read_bytes = fread(&content[0], 1, size, f);
    fclose(f);

    if (read_bytes < (size_t)size) {
        content.resize(read_bytes);
    }
    return true;
}

bool VFS::write_file(const std::string& path, const std::string& content) {
    std::string full_path = prepend_base_path(path);
    FILE* f = fopen(full_path.c_str(), "w");
    if (f == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path.c_str());
        return false;
    }
    size_t written = fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    return written == content.size();
}

bool VFS::append_file(const std::string& path, const std::string& content) {
    std::string full_path = prepend_base_path(path);
    FILE* f = fopen(full_path.c_str(), "a");
    if (f == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", full_path.c_str());
        return false;
    }
    size_t written = fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    return written == content.size();
}

bool VFS::delete_file(const std::string& path) {
    std::string full_path = prepend_base_path(path);
    if (unlink(full_path.c_str()) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", full_path.c_str());
        return false;
    }
    return true;
}

bool VFS::replace_in_file(const std::string& path, const std::string& target, const std::string& replacement) {
    if (target.empty()) return false;
    std::string content;
    if (!read_file(path, content)) {
        return false;
    }
    size_t pos = 0;
    bool replaced = false;
    while ((pos = content.find(target, pos)) != std::string::npos) {
        content.replace(pos, target.length(), replacement);
        pos += replacement.length();
        replaced = true;
    }
    if (replaced) {
        return write_file(path, content);
    }
    return true; // No target found, file remains unchanged but operation succeeds
}

long VFS::get_file_size(const std::string& path) {
    std::string full_path = prepend_base_path(path);
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
        return st.st_size;
    }
    return -1;
}

bool VFS::list_dir(const std::string& path, std::vector<std::string>& files) {
    std::string full_path = prepend_base_path(path);
    DIR* dir = opendir(full_path.c_str());
    if (dir == nullptr) {
        ESP_LOGE(TAG, "Failed to open directory: %s", full_path.c_str());
        return false;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        files.push_back(entry->d_name);
    }
    closedir(dir);
    return true;
}
