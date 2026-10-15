#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
inline void *heap_caps_malloc(size_t n,int){return std::malloc(n);}
inline void heap_caps_free(void *p){std::free(p);}

// Hold the real producer's final close to expose completion-before-close races.
#include <atomic>
#include <cstdio>
#include <thread>
#include <chrono>
inline std::atomic<bool> audio_test_hold_close{false}, audio_test_close_entered{false};
inline int audio_test_close(FILE *file) {
    audio_test_close_entered.store(true);
    while (audio_test_hold_close.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return std::fclose(file);
}
#define SKULL_AUDIO_CLOSE audio_test_close
