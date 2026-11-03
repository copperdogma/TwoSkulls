#pragma once
#include <cstddef>
#include <cstdint>

namespace skull::logexport {
constexpr size_t capacity=14335,chunk_bytes=256;
struct Request { char nonce[17]{}; unsigned slot=0; };
// Strict bounded wire request: GET <16 lowercase hex characters> <0..7>.
bool parse(const char *data,size_t length,Request &request);
uint32_t crc32(const uint8_t *data,size_t length);
// Sole control owner. Storage reader fills one bounded PSRAM snapshot, closes
// the file, then MQTT sends immutable frames; no SD reads during frame sending.
class Transfer {
public:
    void start(const Request &request);
    bool append(const uint8_t *data,size_t length);
    void seal();
    bool frame(const char *&suffix,const uint8_t *&data,size_t &length);
    void sent();
    void cancel();
    bool active()const{return phase_!=Phase::idle;}
    const Request &request()const{return request_;}
private:
    enum class Phase { idle,loading,begin,chunks,end };
    Request request_;
    Phase phase_=Phase::idle;
    size_t length_=0,offset_=0;
    uint32_t checksum_=0;
    uint8_t bytes_[capacity]{};
    char suffix_[48]{},text_[96]{};
};
}
