#pragma once
#include <chrono>
#include <cstdio>
#include <string>

namespace savr::testarea::diagnostics {
// Low-frequency events only. Keep one previous file, including in player builds.
inline bool Append(const char* path,const char* line,long limit=512*1024) {
    FILE* file=std::fopen(path,"a");
    if(!file)return false;
    if(std::fseek(file,0,SEEK_END)==0&&std::ftell(file)>=limit) {
        std::fclose(file);
        const std::string previous=std::string(path)+".previous";
        std::remove(previous.c_str());
        const bool rotated=std::rename(path,previous.c_str())==0;
        file=std::fopen(path,rotated?"a":"w");
        if(!file)return false;
    }
    const auto time=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const bool written=std::fprintf(file,"%lld %s\n",static_cast<long long>(time),line)>=0;
    const bool closed=std::fclose(file)==0;
    return written&&closed;
}
}
