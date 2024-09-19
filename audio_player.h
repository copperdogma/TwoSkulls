#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "SD.h"
#include "sd_card_manager.h"
#include "SoundData.h" // For Frame definition
#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <stdint.h>
#include <Arduino.h>

// Constants for file index markers
const uint16_t SAME_FILE = 0xFFFF;
const uint16_t END_OF_FILE = 0xFFFE;

// Define a constant for undefined buffer end position
const size_t BUFFER_END_POS_UNDEFINED = (size_t)(-1);

// Define FileEntry struct
struct FileEntry {
    String filePath;
    size_t bufferEndPos;

    FileEntry(const String &path = "", size_t end = BUFFER_END_POS_UNDEFINED)
        : filePath(path), bufferEndPos(end) {}
};

class AudioPlayer
{
public:
    AudioPlayer(SDCardManager *sdCardManager);
    void begin();
    void playNext(const char *filePath);
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);
    bool isAudioPlaying() const;
    void setMuted(bool muted);
    unsigned long getPlaybackTime() const;
    String getCurrentlyPlayingFilePath() const;
    bool hasRemainingAudioData();

    // Callback types
    typedef void (*PlaybackCallback)(const String &filePath);
    typedef void (*AudioFramesProvidedCallback)(const String&, const Frame*, int32_t);

    // Setters for callbacks
    void setPlaybackStartCallback(PlaybackCallback callback) { m_playbackStartCallback = callback; }
    void setPlaybackEndCallback(PlaybackCallback callback) { m_playbackEndCallback = callback; }
    void setAudioFramesProvidedCallback(AudioFramesProvidedCallback callback) { m_audioFramesProvidedCallback = callback; }

private:
    void fillBuffer();
    bool startNextFile();
    uint16_t getFileIndex(const String &filePath);
    String getFilePath(uint16_t fileIndex);
    uint16_t addFileToList(const String &filePath);

    // Updated writeToBuffer signature
    void writeToBuffer(const uint8_t *audioData, size_t dataSize);

    // Buffer management
    static const size_t AUDIO_BUFFER_SIZE = 8192; // Adjust as needed
    uint8_t m_audioBuffer[AUDIO_BUFFER_SIZE];
    size_t m_writePos;
    size_t m_readPos;
    size_t m_bufferFilled;

    // Playback state
    File audioFile;
    String m_currentFilePath;
    uint16_t m_currentBufferFileIndex;
    uint16_t m_currentPlaybackFileIndex;
    bool m_isAudioPlaying;
    bool m_muted;

    // Timing
    unsigned long m_currentPlaybackTime;
    unsigned long m_lastFrameTime;

    // Total bytes read (for debugging)
    size_t m_totalBytesRead;

    // File list management
    std::vector<FileEntry> m_fileList;

    // Audio queue
    std::queue<std::string> audioQueue;

    // SD card manager
    SDCardManager *m_sdCardManager;

    // Thread safety
    std::mutex m_mutex;

    // Callbacks
    PlaybackCallback m_playbackStartCallback;
    PlaybackCallback m_playbackEndCallback;
    AudioFramesProvidedCallback m_audioFramesProvidedCallback;
};

#endif // AUDIO_PLAYER_H