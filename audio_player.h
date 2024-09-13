#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include <mutex>
#include <string>
#include "BluetoothA2DPSource.h"
#include "SD.h"

#define AUDIO_BUFFER_SIZE 32768  // Increased buffer size

class AudioPlayer {
public:
    AudioPlayer();
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

    void fillBuffer();
    bool startNextFile();
};

#endif // AUDIO_PLAYER_H