#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <queue>
#include "BluetoothA2DPSource.h"
#include "FS.h"
#include "SD.h"
#include <vector>
#include <string>
#include <arduinoFFT.h>
#include "esp32-hal-ledc.h"  // Include ESP32 specific PWM library
#include "servo_controller.h"
#include "parsed_skit.h"

struct AudioSegment {
  unsigned long start;
  unsigned long duration;
  bool shouldPlay;
};

class AudioPlayer {
public:
  static const int SAMPLES = 256;
  static const int SAMPLE_RATE = 44100;

  AudioPlayer(ServoController& servoController);  // Modified constructor
  void begin();
  void update();
  void updateSkit();  // Add this line
  uint8_t* getCurrentAudioBuffer();
  size_t getCurrentAudioBufferSize();
  void playNow(const char* filePath);
  void playNext(const char* filePath);
  void playSkitNext(const ParsedSkit& skit);
  bool isCurrentlyPlaying();
  bool isPlayingSkit() const;
  const ParsedSkit& getCurrentSkit() const;
  size_t getCurrentSkitLine() const;
  bool hasStartedPlaying() const;
  bool isBluetoothConnected() const;
  void setBluetoothConnected(bool connected);
  void setAudioReadyToPlay(bool ready);
  static int32_t provideAudioFrames(Frame* frame, int32_t frame_count);
  double calculateRMS(const int16_t* samples, int numSamples);
  void performFFT();
  double getFFTResult(int index);
  ParsedSkit parseSkitFile(const String& wavFile, const String& txtFile);
  ParsedSkit findSkitByName(const std::vector<ParsedSkit>& skits, const String& name);
  void prepareSkitPlayback(const ParsedSkit& skit);
  bool fileExists(fs::FS& fs, const char* path);
  void handleBluetoothStateChange(esp_a2d_connection_state_t state);

  void setJawPosition(int position);

private:
  uint8_t* buffer;
  size_t currentBufferSize;
  std::queue<String> audioQueue;
  File currentAudioFile;
  bool shouldPlayNow;
  volatile bool isPlaying;
  bool m_isPlayingSkit;
  ParsedSkit m_currentSkit;
  size_t m_currentSkitLine;
  unsigned long m_skitStartTime;
  bool m_hasStartedPlaying;
  bool m_isBluetoothConnected;
  bool m_isAudioReadyToPlay;

  // Add these FFT-related members
  double vReal[SAMPLES];
  double vImag[SAMPLES];
  arduinoFFT FFT;

  // New private members for skit management
  std::vector<AudioSegment> audioSegments;
  size_t currentSegment;

  // New private method
  void lock();
  void unlock();
  void startPlaying(const char* filePath);
  int32_t _provideAudioFrames(Frame* frame, int32_t frame_count);
  bool hasFinishedPlaying();
  void loadAudioFile(const char* filename);
  void processSkitLine();

  // New private members for jaw movement
  ServoController& m_servoController;  // Reference to ServoController

  // New member to track end of file
  bool m_reachedEndOfFile;

  size_t m_totalFileSize;
  size_t m_totalBytesRead;
  bool m_isEndOfFile;
};

extern AudioPlayer* audioPlayer;

#endif // AUDIO_PLAYER_H