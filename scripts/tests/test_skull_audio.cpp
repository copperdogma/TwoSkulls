// Real producer/provider state machine with host queues and filesystem. Build:
// c++ -std=c++17 -pthread -DSKULL_AUDIO_SD_ROOT='"/tmp/skull-audio-test"' \
// -Iscripts/tests/audio_stubs -Ifirmware/main scripts/tests/test_skull_audio.cpp \
// firmware/main/skull_audio.cpp firmware/main/skull_wave.cpp -o /tmp/test-skull-audio
#include "skull_audio.h"
#include "skull_catalog.h"
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <filesystem>
#include <vector>
#ifndef SKULL_AUDIO_SD_ROOT
#define SKULL_AUDIO_SD_ROOT "/tmp/skull-audio-test"
#endif
namespace skull::catalog {
bool speaking(const Asset &a, char role, uint32_t frame) {
    if (!a.skit) return true;
    for (unsigned i=0;i<a.line_count;++i) if (frame>=a.lines[i].first_frame && frame<a.lines[i].end_frame) return a.lines[i].role==role;
    return false;
}
}
static void wait_until(bool (*f)()) {
    for (unsigned i=0;i<2000 && !f();++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(f());
}
static skull::catalog::Asset asset;
static void fixture(unsigned frames) {
    std::filesystem::create_directories(SKULL_AUDIO_SD_ROOT);
    FILE *f=std::fopen(SKULL_AUDIO_SD_ROOT "/test.wav","wb");assert(f);
    uint8_t header[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0x44,0xac,0,0,0x10,0xb1,2,0,4,0,16,0,'d','a','t','a'};
    uint32_t data=frames*4,size=data+36;
    std::memcpy(header+4,&size,4);std::memcpy(header+40,&data,4);
    std::fwrite(header,1,44,f);
    for(unsigned i=0;i<frames*2;++i){int16_t sample=(i % 2 == 0) ? 1000 : -1000;std::fwrite(&sample,2,1,f);}std::fclose(f);
    std::strcpy(asset.path,"/test.wav");asset.wave={44,data};
}
int main() {
    using namespace skull;
    fixture(10000);asset.skit=true;asset.line_count=2;asset.lines[0]={0,5000,'A'};asset.lines[1]={5000,10000,'B'};
    assert(audio::quiescent());
    audio_test_hold_close.store(true);
    assert(audio::init(50,true)); assert(audio::prepare(&asset)); assert(!audio::start()); wait_until(audio::ready);assert(audio::start());
    uint32_t position=0;
    while(!audio::complete()) {
        uint8_t output[1024]{};auto before=audio::stats().frames;assert(audio::provide(output,sizeof(output)));
        auto after=audio::stats().frames;
        for(unsigned j=0;j<after-before;++j){int16_t left,right;std::memcpy(&left,output+j*4,2);std::memcpy(&right,output+j*4+2,2);assert(left==((position+j<5000)?500:0));assert(right==((position+j<5000)?-500:0));}
        position+=after-before;std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(position==10000);assert(!audio::busy());
    // The last block is consumed while the real worker is held inside fclose.
    // Playback completion alone must not permit an intentional CPU reset.
    wait_until([] { return audio_test_close_entered.load(); });
    assert(!audio::quiescent());
    audio_test_hold_close.store(false);
    wait_until(audio::quiescent);
    const auto timing=audio::stats();assert(timing.blocks==12);assert(audio_test_max_burst.load()==2);
    assert(timing.read_us_total>=timing.read_us_max);
    assert(timing.process_us_total>=timing.process_us_max);
    assert(timing.refill_gap_us_max>0);
    assert(timing.role_frames==5000);assert(timing.input_nonzero_samples==20000);
    assert(timing.output_nonzero_samples==10000);assert(timing.output_peak==500);
    // Hold partial callback block, cancel while callbacks stop, and verify that
    // no reprepare/reset occurs until a provider acknowledges cancellation.
    assert(audio::prepare(&asset));wait_until(audio::ready);assert(audio::start());uint8_t tiny[4]{};assert(audio::provide(tiny,4));audio::cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));assert(audio::stats().cancel_waiting);assert(audio::busy());assert(!audio::quiescent());assert(!audio::prepare(&asset));
    assert(audio::provide(tiny,4));for(unsigned i=0;i<100 && audio::busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!audio::busy());wait_until(audio::quiescent);
    // Rapid provider drains prebuffer before worker refills: starvation preserves
    // source cursor and emits only the caller's zeros.
    assert(audio::prepare(&asset));wait_until(audio::ready);assert(audio::start());std::vector<uint8_t> large(100000,0);assert(audio::provide(large.data(),large.size()));assert(audio::stats().underruns>0);assert(audio::stats().frames<=8*882);
    audio::cancel();audio::provide(tiny,4);wait_until(audio::quiescent);
    // Failed open must clear worker file activity rather than trapping reboot.
    std::strcpy(asset.path,"/missing.wav");assert(audio::prepare(&asset));
    wait_until(audio::error);wait_until(audio::quiescent);
    std::puts("skull_audio producer, role gain/muting, EOF, cancellation hold, starvation, and reboot file quiescence passed");
    std::fflush(stdout);std::_Exit(0); // worker is intentionally device-lifetime
}
