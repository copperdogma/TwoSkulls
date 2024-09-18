#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include <mutex>
#include <string>
#include <functional>
#include "BluetoothA2DPSource.h"
#include "SD.h"
#include "sd_card_manager.h"
#include "SoundData.h" // For Frame definition
#include <vector>

#define AUDIO_BUFFER_SIZE 32768

// Forward declaration
class SDCardManager;

class AudioPlayer
{
public:
    AudioPlayer(SDCardManager *sdCardManager);
    void begin();
    void playNext(const char *filePath);
    bool hasRemainingAudioData();
    bool isAudioPlaying() const;
    void setMuted(bool muted);
    size_t readAudioDataFromFile(uint8_t *buffer, size_t bytesToRead);
    unsigned long getPlaybackTime() const;
    String getCurrentlyPlayingFilePath() const;
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);

    // New callback types
    using PlaybackStartCallback = std::function<void(const String &)>;
    using PlaybackEndCallback = std::function<void(const String &)>;
    using AudioFramesProvidedCallback = std::function<void(const String &, const Frame *, int32_t)>;

    // New methods to set callbacks
    void setPlaybackStartCallback(PlaybackStartCallback callback) { m_playbackStartCallback = callback; }
    void setPlaybackEndCallback(PlaybackEndCallback callback) { m_playbackEndCallback = callback; }
    void setAudioFramesProvidedCallback(AudioFramesProvidedCallback callback) { m_audioFramesProvidedCallback = callback; }

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
    SDCardManager *m_sdCardManager;

    void writeToBuffer(uint16_t fileIndex, const uint8_t* audioData, size_t dataSize);

    // Freame/file sync tracking
    std::vector<String> m_fileList;
    uint16_t getFileIndex(const String &filePath);
    String getFilePath(uint16_t fileIndex);
    uint16_t m_currentFileIndex = 0;

    // New callback members
    PlaybackStartCallback m_playbackStartCallback;
    PlaybackEndCallback m_playbackEndCallback;
    AudioFramesProvidedCallback m_audioFramesProvidedCallback;

    static const uint16_t SAME_FILE = 0;
    static const uint16_t END_OF_FILE = UINT16_MAX;

    void fillBuffer();
    bool startNextFile();

    uint16_t addFileToList(const String &filePath);
};

#endif // AUDIO_PLAYER_H