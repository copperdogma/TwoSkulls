#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include "BluetoothA2DPSource.h"
#include "FS.h"
#include "SD.h"
#include <vector>
#include <string>

struct ParsedSkitLine {
  char speaker;
  unsigned long timestamp;
  unsigned long duration;
  float jawPosition;
};

struct ParsedSkit {
  String audioFile;
  String txtFile;
  std::vector<ParsedSkitLine> lines;
};

typedef ParsedSkit Skit; // Add this line to create an alias for ParsedSkit

class AudioPlayer;
extern AudioPlayer* audioPlayer;

class AudioPlayer {
private:
  uint8_t* buffer = nullptr;     // Dynamic audio buffer
  size_t currentBufferSize = 0;  // Current size of the dynamic audio buffer
  std::queue<String> audioQueue;
  File currentAudioFile;
  bool shouldPlayNow = false;
  volatile bool isPlaying = false;
  bool m_isPlayingSkit;
  ParsedSkit m_currentSkit;
  size_t m_currentSkitLine;
  unsigned long m_skitStartTime;
  bool m_hasStartedPlaying;
  bool m_isBluetoothConnected;
  bool m_isAudioReadyToPlay;

  void lock();
  void unlock();
  void startPlaying(const char* filePath);
  int32_t _provideAudioFrames(Frame* frame, int32_t frame_count);
  bool hasFinishedPlaying();

public:
  AudioPlayer() {}
  uint8_t* getCurrentAudioBuffer();  // Expose the audio buffer
  size_t getCurrentAudioBufferSize();
  void playNow(const char* filePath);
  void playNext(const char* filePath);
  bool isCurrentlyPlaying();
  static int32_t provideAudioFrames(Frame* frame, int32_t frame_count);
  void update();
  void playSkitNext(const ParsedSkit& skit);
  bool isPlayingSkit() const;
  const ParsedSkit& getCurrentSkit() const;
  size_t getCurrentSkitLine() const;
  bool hasStartedPlaying() const;
  bool isBluetoothConnected() const;
  void setBluetoothConnected(bool connected);
  void setAudioReadyToPlay(bool ready);
};
#endif  // AUDIO_PLAYER_H