#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

#include "servo_controller.h"
#include "arduinoFFT.h"
#include "light_controller.h"
#include "parsed_skit.h"
#include <vector>
#include <Arduino.h>
#include "SoundData.h" // Include this to get the Frame struct definition

// TODO: Should probably be defined by the audioPlayer and passed in from it, either during init or via processAudioFrames()
#define SAMPLES 256
#define SAMPLE_RATE 44100

// Forward declarations
class SDCardManager;

class SkullAudioAnimator
{
public:
    SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController,
                       std::vector<ParsedSkit> &skits, SDCardManager &sdCardManager, int servoMinDegrees, int servoMaxDegrees);

    ParsedSkit findSkitByName(const std::vector<ParsedSkit> &skits, const String &name);
    bool isCurrentlySpeaking() { return m_isCurrentlySpeaking; }

    void onPlaybackStart(const String &filePath);
    void onPlaybackEnd(const String &filePath);

    void processAudioFrames(const Frame* frames, int32_t frameCount, const String& currentFile, unsigned long playbackTime);

private:
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

    String m_currentFile;
    unsigned long m_currentPlaybackTime;
    bool m_isAudioPlaying;

    void updateJawPosition(const Frame* frames, int32_t frameCount);
    void updateEyes();
    void updateSkit();
    double calculateRMS(const int16_t *samples, int numSamples);
    void performFFT(const Frame* frames, int32_t frameCount);
    double getFFTResult(int index);

    int m_servoMinDegrees;
    int m_servoMaxDegrees;
};

#endif // SKULL_AUDIO_ANIMATOR_H