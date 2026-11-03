#pragma once
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
namespace skull::faultlog {
// First milestones survive for this boot; bounded recent events wrap separately.
struct Lifecycle {
    struct Metrics {
        int64_t control_ready_ms=-1,sd_mount_ms=-1,sd_config_ms=-1,speaker_connected_ms=-1;
        int64_t bluetooth_rssi_delta=-999;
        uint32_t sd_attempts=0,sd_failures=0,speaker_disconnects=0;
    } metrics;
    static int64_t number(const char *line,const char *key,int64_t fallback=-1){
        const char *value=std::strstr(line,key);if(!value)return fallback;
        value+=std::strlen(key);char *end=nullptr;const auto result=std::strtoll(value,&end,10);
        return end==value?fallback:result;
    }
    static constexpr size_t bytes=192, events=12, milestones=14;
    char first[milestones][bytes]{};
    char recent[events][bytes]{};
    char last_wifi[bytes]{}, last_speaker_signal[bytes]{}, last_disconnect[bytes]{};
    size_t head=0,count=0;
    unsigned overwritten=0;
    static const char *key(size_t i) {
        const char *keys[milestones]={"boot","sd_mount_result","sd_result","config_ready",
            "radio_init","radio_start","control_ready","startup_finished","local_ready",
            "ready","speaker_connected","speaker_stream_ready","sd_mount_success","sd_config_success"};
        return keys[i];
    }
    static bool selected(const char *line){return std::strstr(line,") lifecycle: event=")!=nullptr;}
    void capture(const char *line){
        const char *start=std::strstr(line,") lifecycle: event=");if(!start)return;
        start+=std::strlen(") lifecycle: event=");
        for(size_t i=0;i<milestones;++i){
            const size_t n=std::strlen(key(i));
            if(!first[i][0]&&!std::strncmp(start,key(i),n)&&(start[n]==' '||start[n]=='\0')){
                copy(first[i],line);break;
            }
        }
        // Keep successful recovery latency even if the first attempt failed.
        const bool success=std::strstr(start," result=ESP_OK")||std::strstr(start," result=OK");
        if(success&&!std::strncmp(start,"sd_mount_result ",16)&&!first[12][0])copy(first[12],line);
        if(success&&!std::strncmp(start,"sd_result ",10)&&!first[13][0])copy(first[13],line);
        if(!std::strncmp(start,"wifi_sample ",12))copy(last_wifi,line);
        if(!std::strncmp(start,"speaker_rssi ",13))copy(last_speaker_signal,line);
        if(!std::strncmp(start,"speaker_disconnected ",21))copy(last_disconnect,line);
        if(!std::strncmp(start,"control_ready ",14)&&metrics.control_ready_ms<0)
            metrics.control_ready_ms=number(start,"boot_ms=");
        if(success&&!std::strncmp(start,"sd_mount_result ",16)&&metrics.sd_mount_ms<0)
            metrics.sd_mount_ms=number(start,"duration_ms=");
        if(!std::strncmp(start,"sd_result ",10)){
            const auto attempt=number(start,"attempt=",0);
            if(attempt>0)metrics.sd_attempts=uint32_t(attempt);
            if(success&&metrics.sd_config_ms<0)metrics.sd_config_ms=number(start,"duration_ms=");
            if(!success&&metrics.sd_failures!=UINT32_MAX)++metrics.sd_failures;
        }
        if(!std::strncmp(start,"speaker_connected ",18)&&metrics.speaker_connected_ms<0)
            metrics.speaker_connected_ms=number(start,"boot_ms=");
        if(!std::strncmp(start,"speaker_disconnected ",21)){
            metrics.bluetooth_rssi_delta=-999;
            metrics.speaker_disconnects=uint32_t(number(start,"disconnects=",metrics.speaker_disconnects));
        }
        if(!std::strncmp(start,"speaker_rssi ",13))
            metrics.bluetooth_rssi_delta=std::strstr(start,"state=sampled")?number(start,"rssi_delta=",-999):-999;
        copy(recent[head],line);head=(head+1)%events;
        if(count<events)++count;else if(overwritten!=~0u)++overwritten;
    }
private:
    static void copy(char *to,const char *from){
        const size_t n=std::min(std::strlen(from),bytes-1);std::memcpy(to,from,n);to[n]=0;
    }
};
}
