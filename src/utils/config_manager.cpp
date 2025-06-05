#include "config_manager.h"
#include <fstream>
#include <iostream>

ConfigManager::ConfigManager(const std::string& config_path)
    : config_path_(config_path) {}

bool ConfigManager::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(config_path_);
    if (!file) return false;
    try {
        file >> config_;
        return true;
    } catch (...) {
        return false;
    }
}

bool ConfigManager::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(config_path_);
    if (!file) return false;
    try {
        file << config_.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

template<typename T>
T ConfigManager::get(const std::string& key, T default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.contains(key) && !config_[key].is_null()) {
        try {
            return config_.at(key).get<T>();
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

template<typename T>
void ConfigManager::set(const std::string& key, T value) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_[key] = value;
}

template<typename T>
std::optional<T> ConfigManager::getOptional(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.contains(key) && !config_[key].is_null()) {
        try {
            return config_.at(key).get<T>();
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// Instanciar explícitamente plantillas usadas
template int ConfigManager::get(const std::string&, int) const;
template double ConfigManager::get(const std::string&, double) const;
template bool ConfigManager::get(const std::string&, bool) const;
template std::string ConfigManager::get(const std::string&, std::string) const;

template void ConfigManager::set(const std::string&, int);
template void ConfigManager::set(const std::string&, double);
template void ConfigManager::set(const std::string&, bool);
template void ConfigManager::set(const std::string&, std::string);

template std::optional<int> ConfigManager::getOptional(const std::string&) const;
template std::optional<double> ConfigManager::getOptional(const std::string&) const;
template std::optional<bool> ConfigManager::getOptional(const std::string&) const;
template std::optional<std::string> ConfigManager::getOptional(const std::string&) const;

std::optional<std::string> ConfigManager::getStringOptional(const std::string& key) const {
    return getOptional<std::string>(key);
}
std::optional<int> ConfigManager::getIntOptional(const std::string& key) const {
    return getOptional<int>(key);
}
std::optional<bool> ConfigManager::getBoolOptional(const std::string& key) const {
    return getOptional<bool>(key);
}

// ---- Static convenience wrappers ----
std::string ConfigManager::getString(const std::string& key,
                                     const std::string& default_value) {
    return ConfigManager::getInstance().get<std::string>(key, default_value);
}

float ConfigManager::getFloat(const std::string& key, float default_value) {
    // Reuse double getter to avoid explicit float template instantiation
    return static_cast<float>(ConfigManager::getInstance().get<double>(key,
                                                            default_value));
}
