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
    SkullAudioAnimator(ServoController& servoController, LightController& lightController);
    void begin();
    void update();
    void playNow(const char* filePath);
    void playNext(const char* filePath);
    void playSkitNext(const ParsedSkit& skit);
    bool isCurrentlyPlaying();
    bool isPlayingSkit() const;
    bool hasFinishedPlaying();
    void setBluetoothConnected(bool connected);
    void setAudioReadyToPlay(bool ready);
    ParsedSkit findSkitByName(const std::vector<ParsedSkit>& skits, const String& name);
    size_t getTotalBytesRead() const;
    void logState();
    bool fileExists(fs::FS &fs, const char* path);
    int32_t provideAudioFrames(Frame* frame, int32_t frame_count);
    ParsedSkit parseSkitFile(const String& wavFile, const String& txtFile);
    const ParsedSkit& getCurrentSkit() const;

private:
    AudioPlayer m_audioPlayer;
    ServoController& m_servoController;
    LightController& m_lightController;
    bool m_isPlayingSkit;
    size_t m_currentSkitLine;
    ParsedSkit m_currentSkit;
    double vReal[SAMPLES];
    double vImag[SAMPLES];
    arduinoFFT FFT;

    void updateJawPosition();
    void updateEyes();  // Add this line
    void updateSkit();
    double calculateRMS(const int16_t* samples, int numSamples);
    void performFFT();
    double getFFTResult(int index);
};

#endif // SKULL_AUDIO_ANIMATOR_H