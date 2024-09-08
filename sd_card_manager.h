#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include "FS.h"
#include "SD.h"
#include "skull_audio_animator.h"  // Change this from audio_player.h
#include <vector>

struct SDCardContent {
    std::vector<ParsedSkit> skits;
    std::vector<String> audioFiles;
    String primaryInitAudio;
    String secondaryInitAudio;
};

class SDCardManager {
public:
    SDCardManager(SkullAudioAnimator* skullAudioAnimator);  // Change this line
    bool begin();
    SDCardContent loadContent();

private:
    SkullAudioAnimator* m_skullAudioAnimator;  // Change this line
    bool processSkitFiles(SDCardContent& content);
};

#endif // SD_CARD_MANAGER_H