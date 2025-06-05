#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>
#include <mutex>
#include <optional>

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance("config.json");
        return instance;
    }
    static ConfigManager& getInstance(const std::string& config_path) {
        static ConfigManager instance(config_path);
        return instance;
    }

    ConfigManager(const ConfigManager&) = delete;
    void operator=(const ConfigManager&) = delete;

    bool load();
    bool save();

    template<typename T>
    T get(const std::string& key, T default_value) const;

    template<typename T>
    void set(const std::string& key, T value);

    template<typename T>
    std::optional<T> getOptional(const std::string& key) const;

    std::optional<std::string> getStringOptional(const std::string& key) const;
    std::optional<int> getIntOptional(const std::string& key) const;
    std::optional<bool> getBoolOptional(const std::string& key) const;

    std::string getStatusFilePath() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_file_path;
    }
    void setStatusFilePath(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_file_path = path;
    }

    void setConfigPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_path_ = path;
    }
    std::string getConfigPath() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_path_.string();
    }

private:
    explicit ConfigManager(const std::string& config_path = "config.json");

    std::filesystem::path config_path_;
    nlohmann::json config_;
    mutable std::mutex mutex_;
    std::string status_file_path = "zarbackend/zartrux_status.json";
};
