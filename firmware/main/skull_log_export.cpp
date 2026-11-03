#include "skull_log_export.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace skull::logexport {
bool parse(const char *data,size_t length,Request &request){
    request={};
    if(!data||length!=22||std::memcmp(data,"GET ",4)||data[20]!=' '||data[21]<'0'||data[21]>'7')return false;
    for(size_t i=4;i<20;++i)if(!((data[i]>='0'&&data[i]<='9')||(data[i]>='a'&&data[i]<='f')))return false;
    std::memcpy(request.nonce,data+4,16);request.slot=unsigned(data[21]-'0');return true;
}
uint32_t crc32(const uint8_t *data,size_t length){
    uint32_t crc=~uint32_t(0);
    for(size_t i=0;i<length;++i){
        crc^=data[i];
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}
void Transfer::start(const Request &request){request_=request;length_=offset_=0;phase_=Phase::loading;}
bool Transfer::append(const uint8_t *data,size_t length){
    if(phase_!=Phase::loading||(!data&&length)||length>capacity-length_)return false;
    if(length)std::memcpy(bytes_+length_,data,length);
    length_+=length;return true;
}
void Transfer::seal(){if(phase_==Phase::loading){checksum_=crc32(bytes_,length_);phase_=Phase::begin;}}
bool Transfer::frame(const char *&suffix,const uint8_t *&data,size_t &length){
    if(phase_==Phase::idle||phase_==Phase::loading)return false;
    if(phase_==Phase::begin){
        std::strcpy(suffix_,"begin");
        const int n=std::snprintf(text_,sizeof(text_),"{\"slot\":%u,\"bytes\":%u}",request_.slot,unsigned(length_));
        if(n<0||size_t(n)>=sizeof(text_))return false;
        data=reinterpret_cast<const uint8_t*>(text_);length=size_t(n);
    }else if(phase_==Phase::chunks){
        std::snprintf(suffix_,sizeof(suffix_),"chunk/%u",unsigned(offset_));
        data=bytes_+offset_;length=std::min<size_t>(chunk_bytes,length_-offset_);
    }else{
        std::strcpy(suffix_,"end");
        const int n=std::snprintf(text_,sizeof(text_),"{\"bytes\":%u,\"crc32\":\"%08lx\"}",unsigned(length_),static_cast<unsigned long>(checksum_));
        if(n<0||size_t(n)>=sizeof(text_))return false;
        data=reinterpret_cast<const uint8_t*>(text_);length=size_t(n);
    }
    suffix=suffix_;return true;
}
void Transfer::sent(){
    if(phase_==Phase::begin)phase_=length_?Phase::chunks:Phase::end;
    else if(phase_==Phase::chunks){offset_+=std::min<size_t>(chunk_bytes,length_-offset_);if(offset_==length_)phase_=Phase::end;}
    else if(phase_==Phase::end)cancel();
}
void Transfer::cancel(){phase_=Phase::idle;length_=offset_=0;}
}
