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
    void playNow(const char* filePath);
    void playNext(const char* filePath);
    bool hasRemainingAudioData();
    bool isAudioPlaying() const;
    size_t readAudioDataFromFile(uint8_t* buffer, size_t bytesToRead);
    unsigned long getPlaybackTime() const;
    String getCurrentlyPlayingFilePath() const;
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);

private:
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
    std::mutex m_mutex;
    std::queue<std::string> audioQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    unsigned long m_playbackStartTime;
    unsigned long m_currentFileStartTime;
    String m_currentFilePath;

    void writeAudioDataToBuffer();
    bool shouldPlayAudio();
    void fillBuffer();
    bool startNextFile();
};

#endif // AUDIO_PLAYER_H