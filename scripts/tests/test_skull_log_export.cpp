#include "skull_log_export.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
using namespace skull::logexport;
int main(){
    Request request;
    assert(parse("GET 0123456789abcdef 7",22,request));
    assert(request.slot==7&&!std::strcmp(request.nonce,"0123456789abcdef"));
    for(const char *invalid:{"GET 0123456789abcdef 8","GET 0123456789ABCDEF 1","GET 0123456789abcdef 1 ","GET ../../config.txt 1","GET 0123456789abcdef 1\n"})
        assert(!parse(invalid,std::strlen(invalid),request));
    assert(!parse(nullptr,22,request));
    assert(crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);
    assert(crc32(nullptr,0)==0);
    assert(parse("GET 0123456789abcdef 2",22,request));
    Transfer transfer;
    std::vector<uint8_t> input(capacity);for(size_t i=0;i<input.size();++i)input[i]=uint8_t(i);
    transfer.start(request);
    assert(transfer.append(input.data(),input.size()));assert(!transfer.append(input.data(),1));
    transfer.seal();std::vector<uint8_t> output;unsigned frames=0;
    while(transfer.active()){
        const char *suffix;const uint8_t *data;size_t size;
        assert(transfer.frame(suffix,data,size));std::string previous(reinterpret_cast<const char*>(data),size),previous_suffix=suffix;
        // Failed enqueue must leave this exact immutable frame available.
        const char *suffix2;const uint8_t *data2;size_t size2;
        assert(transfer.frame(suffix2,data2,size2));assert(previous_suffix==suffix2&&previous==std::string(reinterpret_cast<const char*>(data2),size2));
        if(!std::strncmp(suffix,"chunk/",6)){assert(size<=256);assert(std::stoul(suffix+6)==output.size());output.insert(output.end(),data,data+size);}
        if(!std::strcmp(suffix,"end"))assert(previous.find("\"crc32\":")!=std::string::npos);
        transfer.sent();++frames;
    }
    assert(output==input&&frames==2+(capacity+255)/256);
    transfer.start(request);transfer.seal();
    const char *suffix;const uint8_t *data;size_t size;
    assert(transfer.frame(suffix,data,size)&&!std::strcmp(suffix,"begin"));transfer.sent();
    assert(transfer.frame(suffix,data,size)&&!std::strcmp(suffix,"end"));
    assert(std::string(reinterpret_cast<const char*>(data),size).find("00000000")!=std::string::npos);transfer.sent();assert(!transfer.active());
    transfer.start(request);assert(transfer.append(input.data(),2));transfer.cancel();assert(!transfer.frame(suffix,data,size));
    transfer.start(request);assert(transfer.append(input.data(),1));transfer.seal();transfer.sent();
    assert(transfer.frame(suffix,data,size)&&size==1);transfer.cancel();
    std::puts("log export request/bounds/CRC/framing/backpressure tests passed");
}
