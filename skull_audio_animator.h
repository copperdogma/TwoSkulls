#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

#include "audio_player.h"
#include "servo_controller.h"
#include "arduinoFFT.h"
#include "file_manager.h"
#include "light_controller.h"  // Add this line
#include <vector>

#define SAMPLES 256
#define SAMPLE_RATE 44100

class SkullAudioAnimator {
public:
    SkullAudioAnimator(bool isPrimary, ServoController& servoController, LightController& lightController, 
        std::vector<ParsedSkit>& skits);
    void begin();
    void update();
    void playNow(const char* filePath);
    void playNext(const char* filePath);
    void playSkitNext(const ParsedSkit& skit);
    void setBluetoothConnected(bool connected);
    ParsedSkit findSkitByName(const std::vector<ParsedSkit>& skits, const String& name);
    size_t getTotalBytesRead() const;
    int32_t provideAudioFrames(Frame* frame, int32_t frame_count);
    ParsedSkit parseSkitFile(const String& wavFile, const String& txtFile);
    const ParsedSkit& getCurrentSkit() const;
    AudioPlayer& getAudioPlayer() { return m_audioPlayer; }

private:
    AudioPlayer m_audioPlayer;
    ServoController& m_servoController;
    LightController& m_lightController;
    bool m_isPrimary;
    std::vector<ParsedSkit>& m_skits;
    String m_currentAudioFilePath;
    bool m_isCurrentlySpeaking;
    size_t m_currentSkitLineNumber;
    ParsedSkit m_currentSkit;
    double vReal[SAMPLES];
    double vImag[SAMPLES];
    arduinoFFT FFT;

    void updateJawPosition();
    void updateEyes();
    void updateSkit();
    double calculateRMS(const int16_t* samples, int numSamples);
    void performFFT();
    double getFFTResult(int index);
};

#endif // SKULL_AUDIO_ANIMATOR_H