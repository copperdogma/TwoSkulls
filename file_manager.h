#pragma once

#include <SD.h>
#include <vector>
#include "parsed_skit.h"

class FileManager {
public:
    static bool fileExists(const char* path);
    static File openFile(const char* path);
    static String readLine(File& file);
    static ParsedSkit parseSkitFile(const String& wavFile, const String& txtFile);
    static ParsedSkit findSkitByName(const std::vector<ParsedSkit>& skits, const String& name);
    static size_t readFileBytes(File& file, uint8_t* buffer, size_t bufferSize);
    
    // New utility function for file path handling
    static String constructValidPath(const String& basePath, const String& fileName);
    
private:
    static bool isValidPathChar(char c);
};