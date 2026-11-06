#pragma once
#include <cstdint>
#include <cstring>
#include <new>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/mutex.h>
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define MIRROR_LOG(FMT, ...) WHBLogPrintf("[MirrorScreen] " FMT, ##__VA_ARGS__)
inline uint32_t mirrorNowMs(){return static_cast<uint32_t>(OSTicksToMilliseconds(OSGetSystemTime()));}
inline void mirrorSleepMs(uint32_t ms){OSSleepTicks(OSMillisecondsToTicks(ms));}
class MirrorThread {
public:
 using Entry=int(*)(int,void*);
 bool start(Entry entry,uint32_t stackSize,int32_t priority,OSThreadAttributes affinity,const char* name){
  if(running_)return true;
  stack_=new(std::nothrow) uint8_t[stackSize]; if(!stack_)return false;
  std::memset(&thread_,0,sizeof(thread_));
  if(!OSCreateThread(&thread_,entry,0,nullptr,stack_+stackSize,stackSize,priority,affinity)){delete[] stack_;stack_=nullptr;return false;}
  if(name)OSSetThreadName(&thread_,name); OSResumeThread(&thread_); running_=true; return true;
 }
 void join(){if(!running_)return;OSJoinThread(&thread_,nullptr);delete[] stack_;stack_=nullptr;running_=false;}
private: OSThread thread_{}; uint8_t* stack_=nullptr; bool running_=false;
};
