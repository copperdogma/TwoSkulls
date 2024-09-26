#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

// #include "audio_player.h"  // Commented out
#include "servo_controller.h"
#include "arduinoFFT.h"
#include "light_controller.h"
#include "parsed_skit.h"
#include <vector>
#include "SoundData.h"

#define SAMPLES 256
#define SAMPLE_RATE 44100

// Forward declarations
class SDCardManager;

class SkullAudioAnimator
{
public:
    SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController,
                       std::vector<ParsedSkit> &skits, SDCardManager &sdCardManager, int servoMinDegrees, int servoMaxDegrees);

    void update();
    ParsedSkit findSkitByName(const std::vector<ParsedSkit> &skits, const String &name);
    // int32_t provideAudioFrames(Frame *frame, int32_t frame_count);  // Commented out
    // AudioPlayer &getAudioPlayer() { return m_audioPlayer; }  // Commented out
    bool isCurrentlySpeaking() { return m_isCurrentlySpeaking; }

    void onPlaybackStart(const String &filePath);
    void onPlaybackEnd(const String &filePath);

    // Add this new method
    void processAudioFrames(const Frame* frames, int32_t frameCount, const String& currentFile, unsigned long playbackTime);

private:
    // AudioPlayer m_audioPlayer;  // Commented out
    ServoController &m_servoController;
    LightController &m_lightController;
    SDCardManager &m_sdCardManager;
    bool m_isPrimary;
    std::vector<ParsedSkit> &m_skits;
    String m_currentAudioFilePath;
    bool m_isCurrentlySpeaking;
    size_t m_currentSkitLineNumber;
    ParsedSkit m_currentSkit;
    bool m_wasAudioPlaying;
    double vReal[SAMPLES];
    double vImag[SAMPLES];
    arduinoFFT FFT;

    // Add these new members
    String m_currentFile;
    unsigned long m_currentPlaybackTime;
    bool m_isAudioPlaying;

    void updateJawPosition();
    void updateEyes();
    void updateSkit();
    double calculateRMS(const int16_t *samples, int numSamples);
    void performFFT();
    double getFFTResult(int index);

    int m_servoMinDegrees;
    int m_servoMaxDegrees;
};

#endif // SKULL_AUDIO_ANIMATOR_H