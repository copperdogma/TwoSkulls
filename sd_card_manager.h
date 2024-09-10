#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include "FS.h"
#include "SD.h"
#include "skull_audio_animator.h"
#include <vector>

struct SDCardContent {
    std::vector<ParsedSkit> skits;
    std::vector<String> audioFiles;
    String primaryInitAudio;
    String secondaryInitAudio;
};

class SDCardManager {
public:
    SDCardManager(SkullAudioAnimator* skullAudioAnimator);
    bool begin();
    SDCardContent loadContent();

private:
    SkullAudioAnimator* m_skullAudioAnimator;
    bool processSkitFiles(SDCardContent& content);
    ParsedSkit parseSkitFile(const String& wavFile, const String& txtFile);  // Add this line
};

#endif // SD_CARD_MANAGER_H