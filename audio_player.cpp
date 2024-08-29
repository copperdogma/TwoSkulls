#include "audio_player.h"

#include <queue>
#include "BluetoothA2DPSource.h"
#include "FS.h"
#include "SD.h"

// Private implementations --------------------------------------------------

uint8_t* AudioPlayer::getCurrentAudioBuffer() {
  return buffer;
}

size_t AudioPlayer::getCurrentAudioBufferSize() {
  return currentBufferSize;
}

void AudioPlayer::startPlaying(const char* filePath) {
  if (currentAudioFile && strcmp(currentAudioFile.name(), filePath) == 0) {
    // If the current file is already the desired one, return.
    return;
  }
  currentAudioFile.close();
  currentAudioFile = SD.open(filePath);
  if (!currentAudioFile) {
    Serial.printf("Failed to open file: %s\n", filePath);
  } else {
    isPlaying = true;
  }
}

int32_t AudioPlayer::_provideAudioFrames(Frame* frame, int32_t frame_count) {
  if (!isPlaying) return 0;

  if (!currentAudioFile) {
    Serial.println("_provideAudioFrames: currentAudioFile not ready. Exitting early to await data.");
    return 0;
  }

  // Determine the required buffer size
  size_t requiredSize = frame_count * 2 * 2;  // Stereo, 2 channels
  if (requiredSize != currentBufferSize) {
    delete[] buffer;                     // Free the old buffer
    buffer = new uint8_t[requiredSize];  // Allocate a new buffer
    currentBufferSize = requiredSize;    // Update the current buffer size
  }

  size_t bytesRead = currentAudioFile.read(buffer, currentBufferSize);

  if (bytesRead < 1) {
    //Serial.println("_provideAudioFrames: No bytes read. Exitting early to await data.");
    isPlaying = false;
    return 0;
  }

  isPlaying = true;

  size_t currentFilePos = currentAudioFile.position();
  size_t totalFileSize = currentAudioFile.size();

  for (int i = 0, j = 0; i < bytesRead; i += 4, ++j) {
    frame[j].channel1 = (buffer[i + 1] << 8) | buffer[i];
    frame[j].channel2 = (buffer[i + 3] << 8) | buffer[i + 2];
  }

  return bytesRead / 4;
}

bool AudioPlayer::hasFinishedPlaying() {
  // Ideally, this should be tied to the actual audio playback completion event.
  return !currentAudioFile || currentAudioFile.available() == 0;
}


// Public implementations --------------------------------------------------

void AudioPlayer::playNow(const char* filePath) {
  shouldPlayNow = true;
  startPlaying(filePath);
}

void AudioPlayer::playNext(const char* filePath) {
  audioQueue.push(String(filePath));
}

bool AudioPlayer::isCurrentlyPlaying() {
  return isPlaying;
}

int32_t AudioPlayer::provideAudioFrames(Frame* frame, int32_t frame_count) {
  return audioPlayer->_provideAudioFrames(frame, frame_count);
}

void AudioPlayer::update() {
  if (shouldPlayNow) {
    Serial.printf("AudioPlayer: Now playing file: %s\n", currentAudioFile.name());
    startPlaying(currentAudioFile.name());
    shouldPlayNow = false;
  } else if (hasFinishedPlaying()) {
    if (!audioQueue.empty()) {
      const char* nextAudioFile = audioQueue.front().c_str();
      Serial.printf("AudioPlayer: Playing next queued file: %s\n", nextAudioFile);
      startPlaying(nextAudioFile);  // Play the file at the front of the queue
      audioQueue.pop();             // Remove the played file from the queue
    }
  }
}