#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

#include "servo_controller.h"
#include "arduinoFFT.h"
#include "light_controller.h"
#include "parsed_skit.h"
#include <vector>
#include <Arduino.h>
#include "SoundData.h" // Include this to get the Frame struct definition
#include <functional>

// TODO: Should probably be defined by the audioPlayer and passed in from it, either during init or via processAudioFrames()
#define SAMPLES 256
#define SAMPLE_RATE 44100

// Forward declarations
class SDCardManager;

// SkullAudioAnimator class handles the animation of skulls based on audio input
class SkullAudioAnimator
{
public:
    // Constructor: initializes the animator with necessary controllers and parameters
    SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController,
                       std::vector<ParsedSkit> &skits, SDCardManager &sdCardManager, int servoMinDegrees, int servoMaxDegrees);

    // Finds a skit by its name in the list of parsed skits
    ParsedSkit findSkitByName(const std::vector<ParsedSkit> &skits, const String &name);

    // Returns the current speaking state of the skull
    bool isCurrentlySpeaking() { return m_isCurrentlySpeaking; }

    // Main function to process incoming audio frames and update animations
    void processAudioFrames(const Frame *frames, int32_t frameCount, const String &currentFile, unsigned long playbackTime);

    // Typedef for the speaking state callback function
    using SpeakingStateCallback = std::function<void(bool)>;

    // Sets the callback function for speaking state changes
    void setSpeakingStateCallback(SpeakingStateCallback callback);

    // Sets the playback ended state
    void setPlaybackEnded(const String &filePath);

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
    double vReal[SAMPLES];
    double vImag[SAMPLES];
    arduinoFFT FFT;

    String m_currentFile;
    unsigned long m_currentPlaybackTime;
    bool m_isAudioPlaying;

    // Updates the jaw position based on the audio amplitude
    void updateJawPosition(const Frame *frames, int32_t frameCount);

    // Updates the eye brightness based on the speaking state
    void updateEyes();

    // Updates the current skit state and speaking status based on audio playback
    void updateSkit();

    // Calculates the Root Mean Square (RMS) of the audio samples (not currently used)
    double calculateRMS(const int16_t *samples, int numSamples);

    // Performs Fast Fourier Transform on the audio frames (not currently used for animation)
    void performFFT(const Frame *frames, int32_t frameCount);

    // Returns the FFT result for a specific index (not currently used)
    double getFFTResult(int index);

    int m_servoMinDegrees;
    int m_servoMaxDegrees;

    SpeakingStateCallback m_speakingStateCallback;

    // Updates the speaking state and triggers the callback if changed
    void setSpeakingState(bool isSpeaking);

    static const unsigned long SKIT_AUDIO_LINE_OFFSET = 0; // Milliseconds to clip off the end of a skit audio line to avoid overlap
};

#endif // SKULL_AUDIO_ANIMATOR_H