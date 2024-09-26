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
#include "radio_manager.h"

// AudioPlayer class manages audio playback from SD card files
class AudioPlayer
{
public:
    // Constructor initializes the AudioPlayer with SDCardManager and RadioManager
    AudioPlayer(SDCardManager &sdCardManager, RadioManager &radioManager);

    // Add a new audio file to the playback queue
    void playNext(String filePath);

    // Provide audio frames to the audio output stream
    int32_t provideAudioFrames(Frame *frame, int32_t frame_count);

    // Check if audio is currently playing
    bool isAudioPlaying() const;

    // Set the muted state of the audio player
    void setMuted(bool muted);

    // Get the current playback time
    unsigned long getPlaybackTime() const;

    // Get the file path of the currently playing audio
    String getCurrentlyPlayingFilePath() const;

    // Check if there is remaining audio data in the current file
    bool hasRemainingAudioData();

    // Callback types
    typedef void (*PlaybackCallback)(const String &filePath);
    typedef void (*AudioFramesProvidedCallback)(const String &, const Frame *, int32_t);

    // Setters for callbacks
    void setPlaybackStartCallback(PlaybackCallback callback) { m_playbackStartCallback = callback; }
    void setPlaybackEndCallback(PlaybackCallback callback) { m_playbackEndCallback = callback; }
    void setAudioFramesProvidedCallback(AudioFramesProvidedCallback callback) { m_audioFramesProvidedCallback = callback; }

private:
    static constexpr const char* IDENTIFIER = "AudioPlayer";
    static constexpr size_t BUFFER_END_POS_UNDEFINED = static_cast<size_t>(-1);
    static constexpr size_t AUDIO_BUFFER_SIZE = 8192; // Size of the circular audio buffer
    static constexpr unsigned long RADIO_ACCESS_TIMEOUT_MS = 500; // 500ms timeout for radio access

    // FileEntry struct to manage file information
    struct FileEntry
    {
        String filePath;
        size_t bufferEndPos;

        FileEntry(const String &path = "", size_t end = BUFFER_END_POS_UNDEFINED)
            : filePath(path), bufferEndPos(end) {}
    };

    // Fill the audio buffer with data from the current file or start the next file
    void fillBuffer();

    // Start playing the next file in the queue
    bool startNextFile();

    // Get the index of a file in the file list, adding it if it doesn't exist
    uint16_t getFileIndex(const String &filePath);

    // Get the file path for a given file index
    String getFilePath(uint16_t fileIndex);

    // Add a file to the file list if it doesn't exist, and return its index
    uint16_t addFileToList(const String &filePath);

    // Write audio data to the circular buffer
    void writeToBuffer(const uint8_t *audioData, size_t dataSize);

    // Buffer management
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

    // File list management
    std::vector<FileEntry> m_fileList;

    // Audio queue
    std::queue<std::string> audioQueue;

    // SD card manager
    SDCardManager& m_sdCardManager;

    // Thread safety
    std::mutex m_mutex;

    // Callbacks
    PlaybackCallback m_playbackStartCallback;
    PlaybackCallback m_playbackEndCallback;
    AudioFramesProvidedCallback m_audioFramesProvidedCallback;

    RadioManager& m_radioManager;

    void handleEndOfFile();
    void updatePlaybackTime();
};

#endif // AUDIO_PLAYER_H