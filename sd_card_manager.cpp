#include "sd_card_manager.h"

SDCardManager::SDCardManager(SkullAudioAnimator* skullAudioAnimator) : m_skullAudioAnimator(skullAudioAnimator) {}

bool SDCardManager::begin() {
    if (!SD.begin()) {
        Serial.println("SD Card: Mount Failed!");
        return false;
    }
    Serial.println("SD Card: Mounted successfully");
    return true;
}

SDCardContent SDCardManager::loadContent() {
    SDCardContent content;

    // Check for initialization files
    content.primaryInitAudio = "/audio/Initialized - Primary.wav";
    content.secondaryInitAudio = "/audio/Initialized - Secondary.wav";

    Serial.println("Required file '" + content.primaryInitAudio + "' " + 
        String(SD.exists(content.primaryInitAudio) ? "found." : "missing."));
    Serial.println("Required file '" + content.secondaryInitAudio + "' " + 
        String(SD.exists(content.secondaryInitAudio) ? "found." : "missing."));

    processSkitFiles(content);

    return content;
}

bool SDCardManager::processSkitFiles(SDCardContent& content) {
    File root = SD.open("/audio");
    if (!root || !root.isDirectory()) {
        Serial.println("SD Card: Failed to open /audio directory");
        return false;
    }

    Serial.println("Processing skits:");
    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith("Skit") && fileName.endsWith(".wav")) {
            String baseName = fileName.substring(0, fileName.lastIndexOf('.'));
            String txtFileName = baseName + ".txt";
            String fullWavPath = "/audio/" + fileName;
            String fullTxtPath = "/audio/" + txtFileName;

            if (SD.exists(fullTxtPath)) {
                ParsedSkit parsedSkit = parseSkitFile(fullWavPath, fullTxtPath);
                content.skits.push_back(parsedSkit);
                content.audioFiles.push_back(fullWavPath);
                Serial.println("- Processing skit '" + fileName + "' - success. (" + String(parsedSkit.lines.size()) + " lines)");
            } else {
                content.audioFiles.push_back(fullWavPath);
                Serial.println("- Processing skit '" + fileName + "' - WARNING: missing txt file.");
            }
        }
        file = root.openNextFile();
    }

    root.close();
    return true;
}

// Add this method to the SDCardManager class
ParsedSkit SDCardManager::parseSkitFile(const String& wavFile, const String& txtFile) {
    ParsedSkit parsedSkit;
    parsedSkit.audioFile = wavFile;
    parsedSkit.txtFile = txtFile;

    File file = SD.open(txtFile);
    if (!file) {
        Serial.println("Failed to open skit file: " + txtFile);
        return parsedSkit;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
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