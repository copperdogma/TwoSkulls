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
    String getPrimaryMacAddress() const;
    String getSecondaryMacAddress() const;
    int getUltrasonicTriggerDistance() const;
    String getValue(const String& key, const String& defaultValue = "") const;
    int getSpeakerVolume() const { return speakerVolume; }
    void printConfig() const;  // Add this line

private:
    ConfigManager() {}
    std::map<String, String> m_config;
    int speakerVolume;

    void parseConfigLine(const String& line);
};

#endif // CONFIG_MANAGER_H