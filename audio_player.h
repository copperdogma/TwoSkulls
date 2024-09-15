#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include <mutex>
#include <string>
#include "BluetoothA2DPSource.h"
#include "SD.h"

#define AUDIO_BUFFER_SIZE 32768

// Forward declaration
class SDCardManager;

class AudioPlayer {
public:
    AudioPlayer(SDCardManager* sdCardManager);
    void begin();
    void playNext(const char* filePath);
    bool hasRemainingAudioData();
    bool isAudioPlaying() const;
    void setMuted(bool muted);
    size_t readAudioDataFromFile(uint8_t* buffer, size_t bytesToRead);
    unsigned long getPlaybackTime() const;
    String getCurrentlyPlayingFilePath() const;
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);

private:
    File audioFile;
    uint8_t m_audioBuffer[AUDIO_BUFFER_SIZE];
    size_t m_totalBytesRead;
    size_t m_writePos;
    size_t m_readPos;
    size_t m_bufferFilled;
    bool m_isAudioPlaying;
    bool m_muted;
    std::mutex m_mutex;
    std::queue<std::string> audioQueue;
    String m_currentFilePath;
    unsigned long m_currentPlaybackTime;
    unsigned long m_lastFrameTime;
    SDCardManager* m_sdCardManager;

    // Define SAMPLE_RATE as a static constexpr within AudioPlayer
    static constexpr unsigned int SAMPLE_RATE = 44100; // Sample rate in Hz

    // Define unified fade settings
    static constexpr unsigned int FADE_DURATION_MS = 500;         // Total fade duration in milliseconds
    static constexpr float FADE_AGGRESSIVENESS = 0.5f;           // Determines the rate of volume change (1.0f = normal)

    // Calculate FADE_STEP based on duration and aggressiveness
    static constexpr float FADE_STEP_BASE = 1.0f / (FADE_DURATION_MS * SAMPLE_RATE / 1000.0f);
    static constexpr float FADE_STEP = FADE_STEP_BASE * FADE_AGGRESSIVENESS;

    // Members for fade-in and fade-out
    bool m_fadeInInProgress;
    bool m_fadeOutInProgress;
    float m_fadeVolume;

    void fillBuffer();
    bool startNextFile();
    void applyFade(int16_t* samples, int32_t sampleCount);
};

#endif // AUDIO_PLAYER_H