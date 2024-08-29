#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include "BluetoothA2DPSource.h"
#include "FS.h"
#include "SD.h"

class AudioPlayer;
extern AudioPlayer* audioPlayer;

class AudioPlayer {
private:
  uint8_t* buffer = nullptr;     // Dynamic audio buffer
  size_t currentBufferSize = 0;  // Current size of the dynamic audio buffer
  std::queue<String> audioQueue;
  File currentAudioFile;
  bool bluetoothConnected;
  bool audioReadyToPlay;
  bool shouldPlayNow = false;
  volatile bool isPlaying = false;

  void lock();
  void unlock();
  void startPlaying(const char* filePath);
  int32_t _provideAudioFrames(Frame* frame, int32_t frame_count);
  bool hasFinishedPlaying();

public:
  AudioPlayer()
    : bluetoothConnected(false), audioReadyToPlay(false) {}
  void setBluetoothConnected(bool connected);
  void setAudioReadyToPlay(bool ready);
  bool isBluetoothConnected() const;
  bool canStartPlaying() const;
  uint8_t* getCurrentAudioBuffer();  // Expose the audio buffer
  size_t getCurrentAudioBufferSize();
  void playNow(const char* filePath);
  void playNext(const char* filePath);
  bool isCurrentlyPlaying();
  static int32_t provideAudioFrames(Frame* frame, int32_t frame_count);
  void update();
};
#endif  // AUDIO_PLAYER_H