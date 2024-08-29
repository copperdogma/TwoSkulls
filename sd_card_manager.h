#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <vector>
#include "audio_player.h"

struct SDCardContent {
    std::vector<ParsedSkit> skits;
    String primaryInitAudio;
    String secondaryInitAudio;
};

class SDCardManager {
public:
    SDCardManager(AudioPlayer* audioPlayer);
    bool begin();
    SDCardContent loadContent();

private:
    AudioPlayer* audioPlayer;
    bool processSkitFiles(SDCardContent& content);
};

#endif // SD_CARD_MANAGER_H