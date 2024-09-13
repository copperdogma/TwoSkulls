#include "file_manager.h"

bool FileManager::fileExists(const char* path) {
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        return false;
    }
    file.close();
    return true;
}

File FileManager::openFile(const char* path) {
    return SD.open(path);
}

String FileManager::readLine(File& file) {
    return file.readStringUntil('\n');
}

// TODO: move this into the SDCardManager class
// In fact, can we just trasfer all functionality into the SDCardManager class?
ParsedSkit FileManager::parseSkitFile(const String& wavFile, const String& txtFile) {
    ParsedSkit parsedSkit;
    parsedSkit.audioFile = wavFile;
    parsedSkit.txtFile = txtFile;

    File file = openFile(txtFile.c_str());
    if (!file) {
        Serial.println("Failed to open skit file: " + txtFile);
        return parsedSkit;
    }

    while (file.available()) {
        String line = readLine(file);
        line.trim();
        if (line.length() == 0) continue;

        ParsedSkitLine skitLine;
        int commaIndex1 = line.indexOf(',');
        int commaIndex2 = line.indexOf(',', commaIndex1 + 1);
        int commaIndex3 = line.indexOf(',', commaIndex2 + 1);

        skitLine.speaker = line.charAt(0);
        skitLine.timestamp = line.substring(commaIndex1 + 1, commaIndex2).toInt();
        skitLine.duration = line.substring(commaIndex2 + 1, commaIndex3).toInt();

        if (commaIndex3 != -1) {
            skitLine.jawPosition = line.substring(commaIndex3 + 1).toFloat();
        } else {
            skitLine.jawPosition = -1;  // Indicating dynamic jaw movement
        }

        parsedSkit.lines.push_back(skitLine);
    }

    file.close();
    return parsedSkit;
}

ParsedSkit FileManager::findSkitByName(const std::vector<ParsedSkit>& skits, const String& name) {
    for (const auto& skit : skits) {
        if (skit.audioFile.endsWith(name + ".wav")) {
            return skit;
        }
    }
    return { "", "", {} };  // Return empty ParsedSkit if not found
}

size_t FileManager::readFileBytes(File& file, uint8_t* buffer, size_t bufferSize) {
    return file.read(buffer, bufferSize);
}

String FileManager::constructValidPath(const String& basePath, const String& fileName) {
    String result = basePath;
    
    // Ensure base path ends with a separator
    if (result.length() > 0 && result.charAt(result.length() - 1) != '/') {
        result += '/';
    }
    
    // Append fileName without modification
    result += fileName;
    
    return result;
}

bool FileManager::isValidPathChar(char c) {
    // Allow alphanumeric characters, underscore, hyphen, and period
    return isalnum(c) || c == '_' || c == '-' || c == '.';
}