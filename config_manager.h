#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <map>

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    bool loadConfig();
    String getBluetoothSpeakerName() const;  // Renamed from getSpeakerName
    String getRole() const;
    int getUltrasonicTriggerDistance() const;
    String getValue(const String& key, const String& defaultValue = "") const;

private:
    ConfigManager() {}
    std::map<String, String> m_config;

    void parseConfigLine(const String& line);
};

#endif // CONFIG_MANAGER_H