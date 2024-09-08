#include "sd_card_manager.h"

SDCardManager::SDCardManager(SkullAudioAnimator* skullAudioAnimator)
    : m_skullAudioAnimator(skullAudioAnimator) {}

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
    Serial.println("Required file '/audio/Initialized - Primary.wav' " + 
        String(m_skullAudioAnimator->fileExists(SD, "/audio/Initialized - Primary.wav") ? "found." : "missing."));
    Serial.println("Required file '/audio/Initialized - Secondary.wav' " + 
        String(m_skullAudioAnimator->fileExists(SD, "/audio/Initialized - Secondary.wav") ? "found." : "missing."));

    if (m_skullAudioAnimator->fileExists(SD, "/audio/Initialized - Primary.wav")) {
        content.primaryInitAudio = "/audio/Initialized - Primary.wav";
    }
    if (m_skullAudioAnimator->fileExists(SD, "/audio/Initialized - Secondary.wav")) {
        content.secondaryInitAudio = "/audio/Initialized - Secondary.wav";
    }

    processSkitFiles(content);

    return content;
}

bool SDCardManager::processSkitFiles(SDCardContent& content) {
    File root = SD.open("/audio");
    if (!root || !root.isDirectory()) {
        Serial.println("SD Card: Failed to open /audio directory");
        return false;
    }

    std::vector<String> skitFiles;
    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith("Skit") && fileName.endsWith(".wav")) {
            skitFiles.push_back(fileName);
        }
        file = root.openNextFile();
    }

    Serial.println("Processing " + String(skitFiles.size()) + " skits:");
    for (const auto& fileName : skitFiles) {
        String baseName = fileName.substring(0, fileName.lastIndexOf('.'));
        String txtFileName = baseName + ".txt";
        if (m_skullAudioAnimator->fileExists(SD, ("/audio/" + txtFileName).c_str())) {
            ParsedSkit parsedSkit = m_skullAudioAnimator->parseSkitFile("/audio/" + fileName, "/audio/" + txtFileName);
            content.skits.push_back(parsedSkit);
            Serial.println("- Processing skit '" + fileName + "' - success. (" + String(parsedSkit.lines.size()) + " lines)");
        } else {
            Serial.println("- Processing skit '" + fileName + "' - WARNING: missing txt file.");
        }
    }

    return true;
}