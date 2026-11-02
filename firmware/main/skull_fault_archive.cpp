#include "skull_fault_archive.h"
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
namespace skull::faultlog {
bool read_archive(const char *directory,unsigned slot,
                  bool (*consume)(const uint8_t *,size_t,void *),void *context){
    if(!directory||slot>=archive_slots||!consume){errno=EINVAL;return false;}
    char path[160];
    const int n=std::snprintf(path,sizeof(path),"%s/fault-%u.log",directory,slot);
    if(n<=0||size_t(n)>=sizeof(path)){errno=ENAMETOOLONG;return false;}
    FILE *file=std::fopen(path,"rb");if(!file)return false;
    struct stat info{};
    bool ok=fstat(fileno(file),&info)==0;
    if(ok&&(!S_ISREG(info.st_mode)||info.st_size<0)){errno=EINVAL;ok=false;}
    if(ok&&static_cast<uint64_t>(info.st_size)>=report_bytes){errno=EFBIG;ok=false;}
    size_t remaining=ok?static_cast<size_t>(info.st_size):0;
    uint8_t chunk[64];
    while(ok&&remaining){
        const size_t bytes=std::min<size_t>(sizeof(chunk),remaining);
        if(std::fread(chunk,1,bytes,file)!=bytes){if(!errno)errno=EIO;ok=false;break;}
        if(!consume(chunk,bytes,context)){errno=ECANCELED;ok=false;break;}
        remaining-=bytes;
    }
    if(ok&&(std::fgetc(file)!=EOF||std::ferror(file))){errno=EIO;ok=false;}
    // fclose must not hide the original read/consumer failure's diagnostic.
    const int saved_errno=ok?0:errno;
    if(std::fclose(file)!=0)ok=false;
    if(saved_errno)errno=saved_errno;
    return ok;
}
bool archive_report(const char *directory,unsigned slot,const Report &report){
    if(!directory||slot>=archive_slots||report.length>=report_bytes)return false;
    if(mkdir(directory,0755)!=0&&errno!=EEXIST)return false;
    char temp[160],target[160];
    const int a=std::snprintf(temp,sizeof(temp),"%s/pending.tmp",directory);
    const int b=std::snprintf(target,sizeof(target),"%s/fault-%u.log",directory,slot);
    if(a<=0||b<=0||size_t(a)>=sizeof(temp)||size_t(b)>=sizeof(target))return false;
    FILE *file=std::fopen(temp,"wb");if(!file)return false;
    bool ok=std::fwrite(report.text,1,report.length,file)==report.length;
    if(std::fflush(file)!=0)ok=false;
    if(ok&&fsync(fileno(file))!=0)ok=false;
    if(std::fclose(file)!=0)ok=false;
    if(!ok)return false;
    file=std::fopen(temp,"rb");if(!file)return false;
    char check[256];size_t offset=0;
    while(ok&&offset<report.length){const size_t n=std::min(sizeof(check),size_t(report.length)-offset);
        if(std::fread(check,1,n,file)!=n||std::memcmp(check,report.text+offset,n))ok=false;
        offset+=n;
    }
    if(ok&&(std::fgetc(file)!=EOF||std::ferror(file)))ok=false;
    if(std::fclose(file)!=0)ok=false;
    if(!ok)return false;
    // FatFs rename does not replace existing files. A failed replacement keeps
    // the frozen report in RAM/NVS and retries this same slot, never advances.
    if(unlink(target)!=0&&errno!=ENOENT)return false;
    return std::rename(temp,target)==0;
}
}
