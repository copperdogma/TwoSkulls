#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "BluetoothA2DPSource.h"
#include "FS.h"
#include "SD.h"
#include <vector>
#include <string>
#include "parsed_skit.h"

#define AUDIO_BUFFER_SIZE 32768  // Increased buffer size

class AudioPlayer {
public:
    AudioPlayer();
    void begin();
    void update();
    void playNow(const char* filePath);
    void playNext(const char* filePath);
    bool hasRemainingAudioData();
    bool isAudioPlaying() const;
    void setBluetoothConnected(bool connected);
    size_t getTotalBytesRead() const;
    size_t readAudioData(uint8_t* buffer, size_t bytesToRead);
    void incrementTotalBytesRead(size_t bytesRead);
    unsigned long getPlaybackTime() const;
    String getCurrentlyPlayingFilePath() const;
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);

private:
    void audioPlayerTask();
    void playFile(const char* filePath);

    File audioFile;
    uint8_t m_audioBuffer[AUDIO_BUFFER_SIZE];
    size_t m_bufferPosition;
    size_t m_bufferSize;
    size_t m_totalBytesRead;
    size_t m_writePos;
    size_t m_readPos;
    size_t m_bufferFilled;
    bool m_isBluetoothConnected;
    bool m_isAudioPlaying;
    std::queue<std::string> audioQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    bool shouldStop = false;
    bool isPlaying = false;
    unsigned long m_playbackStartTime;
    unsigned long m_currentFileStartTime;
    String m_currentFilePath;

    void writeAudio();
};

#endif // AUDIO_PLAYER_H