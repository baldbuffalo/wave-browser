// webkit_engine.cpp — WKC peer implementations + SDL integration
// All signatures match vendor/webkit-wiiu/include/wkc/wkc*.h exactly.

#include "webkit_engine.h"
#include "font_data.h"

#include <SDL.h>
#include <SDL_ttf.h>

extern "C" {
#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <coreinit/mutex.h>
}

#include <wkc/wkcbase.h>
#include <wkc/wkcpeer.h>   // defines wkcTimeoutProc, wkc_uint64, wkc_int64, all file/timer/thread peers
#include <wkc/wkcgpeer.h>  // defines fontPeerMalloc, fontPeerFree, wkcFontDrawTextPeer
#include <wkc/wkcmpeer.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

// ─── State ───────────────────────────────────────────────────────────────────

static bool           s_available = false;
static int            s_w = 1280, s_h = 720;
static unsigned char* s_pixels   = nullptr;
static SDL_Renderer*  s_renderer = nullptr;
static SDL_Texture*   s_texture  = nullptr;
static bool           s_dirty    = false;
static char           s_url[512] = {};

// ─── Timer ───────────────────────────────────────────────────────────────────
// wkcTimeoutProc returns bool (from wkcpeer.h)

struct WKCTimer {
    wkcTimeoutProc proc;
    void*          data;
    unsigned int   fire_at;
    unsigned int   interval;
    bool           active;
    WKCTimer*      next;
};
static WKCTimer* s_timers     = nullptr;
static OSMutex   s_timer_mtx;
static bool      s_timer_init = false;

static unsigned int now_ms() {
    return (unsigned int)OSTicksToMilliseconds(OSGetTick());
}

// ─── Thread ──────────────────────────────────────────────────────────────────

struct WKCThread {
    OSThread  ost;
    uint8_t*  stack;
    void*   (*fn)(void*);
    void*     arg;
    void*     result;
};

// Use OSThreadSpecificID slot 0 to store WKCThread pointer
#define WKC_THREAD_SLOT ((OSThreadSpecificID)0)

static int thread_trampoline(int, const char**) {
    WKCThread* t = (WKCThread*)OSGetThreadSpecific(WKC_THREAD_SLOT);
    if (t) t->result = t->fn(t->arg);
    return 0;
}

// ─── TLS ─────────────────────────────────────────────────────────────────────

#define TLS_KEYS  32
#define TLS_THRDS 16
struct TLSKey  { bool used; void(*dtor)(void*); };
struct ThrTLS  { void* thr; void* val[TLS_KEYS]; };
static TLSKey  s_tls_keys[TLS_KEYS] = {};
static ThrTLS  s_tls_thr[TLS_THRDS] = {};
static OSMutex s_tls_mtx;

static ThrTLS* tls_get(void* thr) {
    OSLockMutex(&s_tls_mtx);
    for (auto& t : s_tls_thr) if (t.thr==thr)  { OSUnlockMutex(&s_tls_mtx); return &t; }
    for (auto& t : s_tls_thr) if (!t.thr)       { t.thr=thr; OSUnlockMutex(&s_tls_mtx); return &t; }
    OSUnlockMutex(&s_tls_mtx);
    return nullptr;
}

// ─── Font / Offscreen ────────────────────────────────────────────────────────

struct WKCFont { TTF_Font* f; int size; };
struct WKCOff  { unsigned char* bmp; int rb, w, h; bool owned; };
static const unsigned char k_white[4] = {0xFF,0xFF,0xFF,0xFF};

// ─── UTF-16 → UTF-8 ──────────────────────────────────────────────────────────

static int u16_to_u8(const unsigned short* in, int len, char* out, int max) {
    int j=0;
    for (int i=0; i<len && j<max-4; i++) {
        unsigned short c=in[i];
        if      (c<0x80)  { out[j++]=(char)c; }
        else if (c<0x800) { out[j++]=0xC0|(c>>6); out[j++]=0x80|(c&0x3F); }
        else               { out[j++]=0xE0|(c>>12); out[j++]=0x80|((c>>6)&0x3F); out[j++]=0x80|(c&0x3F); }
    }
    out[j]='\0'; return j;
}

// =============================================================================
extern "C" {
// =============================================================================

// ── Timer ────────────────────────────────────────────────────────────────────

bool wkcTimerInitializePeer(bool(*)(unsigned int, void*)) {
    if (!s_timer_init) { OSInitMutex(&s_timer_mtx); s_timer_init=true; }
    return true;
}
void wkcTimerFinalizePeer(void)        {}
void wkcTimerForceTerminatePeer(void)  {}

void* wkcTimerNewPeer(void) {
    auto* t=(WKCTimer*)calloc(1,sizeof(WKCTimer));
    if (!t) return nullptr;
    OSLockMutex(&s_timer_mtx); t->next=s_timers; s_timers=t; OSUnlockMutex(&s_timer_mtx);
    return t;
}
void wkcTimerDeletePeer(void* p) {
    if (!p) return;
    auto* t=(WKCTimer*)p;
    OSLockMutex(&s_timer_mtx);
    t->active=false;
    WKCTimer** pp=&s_timers;
    while (*pp&&*pp!=t) pp=&(*pp)->next;
    if (*pp) *pp=t->next;
    OSUnlockMutex(&s_timer_mtx);
    free(t);
}
bool wkcTimerStartOneShotPeer(void* p, unsigned int ms, wkcTimeoutProc cb, void* d) {
    if (!p) return false;
    auto* t=(WKCTimer*)p;
    OSLockMutex(&s_timer_mtx);
    t->proc=cb; t->data=d; t->fire_at=now_ms()+ms; t->interval=0; t->active=true;
    OSUnlockMutex(&s_timer_mtx);
    return true;
}
bool wkcTimerStartPeriodicPeer(void* p, unsigned int ms, wkcTimeoutProc cb, void* d) {
    if (!p) return false;
    auto* t=(WKCTimer*)p;
    OSLockMutex(&s_timer_mtx);
    t->proc=cb; t->data=d; t->fire_at=now_ms()+ms; t->interval=ms; t->active=true;
    OSUnlockMutex(&s_timer_mtx);
    return true;
}
void wkcTimerCancelPeer(void* p)       { if (p) ((WKCTimer*)p)->active=false; }
void wkcTimerWakeUpPeer(void*)         {}
unsigned int wkcGetTickCountPeer(void) { return now_ms(); }

// ── Thread ───────────────────────────────────────────────────────────────────

bool wkcThreadInitializePeer(void) {
    OSInitMutex(&s_tls_mtx);
    return true;
}
void wkcThreadFinalizePeer(void)             {}
void wkcThreadForceTerminatePeer(void)       {}
void wkcThreadForceTerminateThreadsPeer(void){}
void wkcThreadForceTerminateExceptThreadsPeer(void){}

void* wkcThreadCreatePeer(void*(*fn)(void*), void* arg) {
    auto* t=(WKCThread*)calloc(1,sizeof(WKCThread));
    if (!t) return nullptr;
    t->fn=fn; t->arg=arg;
    t->stack=(uint8_t*)malloc(64*1024);
    if (!t->stack) { free(t); return nullptr; }
    OSCreateThread(&t->ost, thread_trampoline, 0, nullptr,
                   t->stack+64*1024, 64*1024, 16, OS_THREAD_ATTRIB_AFFINITY_ANY);
    OSSetThreadSpecific(WKC_THREAD_SLOT, t);
    OSResumeThread(&t->ost);
    return t;
}
void* wkcThreadCreateForMultiCorePeer(void*(*fn)(void*), void* a) { return wkcThreadCreatePeer(fn,a); }

int wkcThreadJoinPeer(void* p, void** res) {
    if (!p) return -1;
    auto* t=(WKCThread*)p;
    OSJoinThread(&t->ost, nullptr);
    if (res) *res=t->result;
    free(t->stack); free(t);
    return 0;
}
void wkcThreadDetachPeer(void* p)             { if (p) OSDetachThread(&((WKCThread*)p)->ost); }
void wkcThreadSetCurrentThreadNamePeer(const char* n) { if(n) OSSetThreadName(OSGetCurrentThread(),n); }
void* wkcThreadCurrentThreadPeer(void)        { return OSGetCurrentThread(); }
void* wkcThreadGetStackBasePeer(void*)        { return nullptr; }  // not accessible via WUT
unsigned int wkcThreadGetStackSizePeer(void*) { return 64*1024; }
void wkcThreadSetStackSizePeer(void*,unsigned int) {}
int  wkcThreadEqualPeer(void* a, void* b)     { return a==b ? 1 : 0; }
void wkcThreadSuspendPeer(void* t)            { if(t) OSSuspendThread((OSThread*)t); }
void wkcThreadResumePeer(void* t)             { if(t) OSResumeThread((OSThread*)t); }
void wkcThreadSuspendAllThreadsPeer(void)     {}
bool wkcThreadTimedSleepPeer(unsigned int ms) { OSSleepTicks(OSMillisecondsToTicks(ms)); return true; }
void wkcThreadYieldPeer(void)                 { OSYieldThread(); }
void wkcThreadSetMainThreadInfoPeer(void*,void*) {}
void* wkcThreadGetMainThreadPeer(void)        { return nullptr; }
bool wkcThreadIsSuspendedPeer(void*)          { return false; }
void wkcThreadSetMutexPeer(void*,void*)       {}
void* wkcThreadGetMutexPeer(void*)            { return nullptr; }
void wkcThreadSetCondPeer(void*,void*)        {}
void* wkcThreadGetCondPeer(void*)             { return nullptr; }
void wkcThreadStoreRegistersPeer(void*,int)   {}
int  wkcThreadStoreThreadRegistersPeer(void*,void*,int) { return 0; }
void* wkcThreadGetStackPointerPeer(const void*) { return nullptr; }
int  wkcThreadAtomicIncrementPeer(int volatile* v) { return ++(*v); }
int  wkcThreadAtomicDecrementPeer(int volatile* v) { return --(*v); }

int wkcThreadKeyNewPeer(void** out, void(*dtor)(void*)) {
    OSLockMutex(&s_tls_mtx);
    for (int i=0; i<TLS_KEYS; i++) {
        if (!s_tls_keys[i].used) {
            s_tls_keys[i]={true,dtor};
            *out=(void*)(uintptr_t)(i+1);
            OSUnlockMutex(&s_tls_mtx);
            return 0;
        }
    }
    OSUnlockMutex(&s_tls_mtx);
    return -1;
}
void wkcThreadKeyDeletePeer(void* k) {
    int i=(int)(uintptr_t)k-1;
    if (i>=0&&i<TLS_KEYS) { OSLockMutex(&s_tls_mtx); s_tls_keys[i]={false,nullptr}; OSUnlockMutex(&s_tls_mtx); }
}
void wkcThreadKeySetSpecificPeer(void* k, void* v) {
    int i=(int)(uintptr_t)k-1;
    if (i<0||i>=TLS_KEYS) return;
    if (auto* t=tls_get(OSGetCurrentThread())) t->val[i]=v;
}
void* wkcThreadKeyGetSpecificPeer(void* k) {
    int i=(int)(uintptr_t)k-1;
    if (i<0||i>=TLS_KEYS) return nullptr;
    auto* t=tls_get(OSGetCurrentThread());
    return t ? t->val[i] : nullptr;
}

// ── Memory ───────────────────────────────────────────────────────────────────

bool wkcMemoryInitializePeer(wkcPeerMallocProc,wkcPeerFreeProc,wkcPeerReallocProc){ return true; }
bool wkcMemoryIsInitializedPeer(void)    { return true; }
void wkcMemoryFinalizePeer(void)         {}
void wkcMemoryForceTerminatePeer(void)   {}
void wkcMemorySetNotifyNoMemoryProcPeer(void*(*)(unsigned int)){}
void wkcMemorySetNotifyMemoryAllocationErrorProcPeer(void(*)(unsigned int,int)){}
void wkcMemorySetNotifyCrashProcPeer(void(*)(const char*,int,const char*,const char*)){}
void wkcMemorySetNotifyStackOverflowProcPeer(void(*)(bool,unsigned int,unsigned int,unsigned int,void*,void*,void*,const char*,int,const char*)){}
void wkcMemorySetCheckAvailabilityProcPeer(bool(*)(unsigned int,bool)){}
void wkcMemorySetCheckMemoryAllocatableProcPeer(bool(*)(unsigned int,int)){}
void* wkcMemoryNotifyNoMemoryPeer(unsigned int)    { return nullptr; }
void  wkcMemorySetCrashReasonPeer(const char*,int,const char*,const char*){}
void  wkcMemoryNotifyCrashPeer(void)               {}
void  wkcMemoryNotifyStackOverflowPeer(bool,unsigned int,unsigned int,void*,const char*,int,const char*){}
bool  wkcMemoryIsStackOverflowPeer(unsigned int,unsigned int*,void**) { return false; }
bool  wkcMemoryCheckAvailabilityPeer(unsigned int)  { return true; }
void  wkcMemorySetAllocatingForSVGPeer(bool)        {}
void  wkcMemorySetAllocatingForImagesPeer(bool)     {}
void  wkcMemorySetAllocationForAnimatedImage(bool)  {}
bool  wkcMemoryIsAllocatingForSVGPeer(void)         { return false; }
int   wkcMemoryGetAllocationStatePeer(void)         { return 0; }
bool  wkcMemoryIsCrashingPeer(void)                 { return false; }
bool  wkcMemoryCheckMemoryAllocatablePeer(unsigned int,int){ return true; }
void  wkcMemoryNotifyMemoryAllocationErrorPeer(unsigned int,int){}
void  wkcMemoryRegisterGlobalObjPeer(volatile void*,int){}
size_t wkcMemoryGetPageSizePeer(void)               { return 4096; }
void  wkcMemorySetExecutablePeer(void*,size_t,bool) {}
void  wkcMemoryCacheFlushPeer(void*,size_t)         {}
void* wkcMemoryFillMem16Peer(void* d,unsigned short v,unsigned int n){
    auto* p=(unsigned short*)d; for(unsigned int i=0;i<n/2;i++) p[i]=v; return d;
}
unsigned short* wkcMemoryFillRange16Peer(unsigned short* d,unsigned short s,unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int v){
    for(unsigned int r=0;r<h;r++){auto* row=d+(y+r)*s+x;for(unsigned int c=0;c<w;c++)row[c]=(unsigned short)v;}return d;
}
void* wkcMemoryFillMem32Peer(void* d,unsigned int v,unsigned int n){
    auto* p=(unsigned int*)d; for(unsigned int i=0;i<n/4;i++) p[i]=v; return d;
}
unsigned int* wkcMemoryFillRange32Peer(unsigned int* d,unsigned int s,unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int v){
    for(unsigned int r=0;r<h;r++){auto* row=d+(y+r)*s+x;for(unsigned int c=0;c<w;c++)row[c]=v;}return d;
}
void* wkcMemoryMoveMemPeer(void* d,void* s,unsigned int n)        { return memmove(d,s,n); }
void* wkcMemoryCopyAlignedMemPeer(void* d,void* s,unsigned int n) { return memcpy(d,s,n); }

// ── File ─────────────────────────────────────────────────────────────────────
// Signatures match wkcpeer.h exactly: int usage, wkc_uint64, wkc_int64

void*      wkcFileFOpenPeer(int /*usage*/, const char* p, const char* m) { return fopen(p,m); }
int        wkcFileFClosePeer(void* f)                    { return fclose((FILE*)f); }
wkc_uint64 wkcFileFReadPeer(void* b, wkc_uint64 s, wkc_uint64 n, void* f) { return (wkc_uint64)fread(b,(size_t)s,(size_t)n,(FILE*)f); }
wkc_uint64 wkcFileFWritePeer(const void* b, wkc_uint64 s, wkc_uint64 n, void* f) { return (wkc_uint64)fwrite(b,(size_t)s,(size_t)n,(FILE*)f); }
int        wkcFileFSeekPeer(void* f, wkc_int64 o, int w) { return fseek((FILE*)f,(long)o,w); }
wkc_int64  wkcFileFTellPeer(void* f)                     { return (wkc_int64)ftell((FILE*)f); }
bool       wkcFileFEofPeer(void* f)                      { return feof((FILE*)f)!=0; }
char*      wkcFileFGetsPeer(char* s, int n, void* f)     { return fgets(s,n,(FILE*)f); }
int        wkcFileFPutsPeer(const char* s, void* f)      { return fputs(s,(FILE*)f); }
int        wkcFileFFlushPeer(void* f)                    { return fflush((FILE*)f); }
int        wkcFileFErrorPeer(void* f)                    { return ferror((FILE*)f); }
int        wkcFileAccessPeer(const char* p, int m)       { return access(p,m); }
bool       wkcFileUnlinkPeer(const char* p)              { return remove(p)==0; }
int        wkcFileStatPeer(const char* p, struct stat* s){ return stat(p,s); }
int        wkcFileErrnoPeer(void)                        { return errno; }

bool wkcFileMakeAllDirectoriesPeer(const char* path) {
    char t[512]; strncpy(t,path,511);
    for (char* p=t+1;*p;p++) if(*p=='/'){*p='\0';mkdir(t,0777);*p='/';}
    return mkdir(t,0777)==0||errno==EEXIST;
}
int wkcFilePathGetFileNamePeer(const char* p,char* out,int mx){
    const char* s=strrchr(p,'/'); s=s?s+1:p;
    strncpy(out,s,mx-1); return (int)strlen(out);
}
int wkcFilePathByAppendingComponentPeer(const char* p,const char* c,char* out,int mx){
    snprintf(out,mx,"%s/%s",p,c); return (int)strlen(out);
}
int wkcFileHomeDirectoryPathPeer(char* out,int mx){
    strncpy(out,"fs:/vol/external01/wiiu/apps/WaveBrowser",mx-1); return (int)strlen(out);
}
int  wkcFileListDirectoryPeer(const char*,const char*,char**,int,int){ return 0; }
int  wkcFileDirectoryNamePeer(const char* p,char* out,int mx){
    const char* s=strrchr(p,'/');
    if(!s){strncpy(out,".",mx-1);return 1;}
    int n=(int)(s-p);if(n>=mx)n=mx-1;
    strncpy(out,p,n);out[n]='\0';return n;
}
void* wkcFileOpenTemporaryFilePeer(const char*,char* out,int mx){
    strncpy(out,"fs:/vol/external01/wiiu/apps/WaveBrowser/tmp",mx-1);return nullptr;
}
void* wkcFileOpenDirectoryPeer(int,const char*,const char*){ return nullptr; }
void  wkcFileCloseDirectoryPeer(void*)      {}
int   wkcFileReadDirectoryPeer(void*,char*,int){ return 0; }

bool wkcFileInitializePeer(void)     { return true; }
void wkcFileFinalizePeer(void)       {}
void wkcFileForceTerminatePeer(void) {}

// ── Debug ────────────────────────────────────────────────────────────────────

bool wkcDebugPrintInitializePeer(void)     { return true; }
void wkcDebugPrintFinalizePeer(void)       {}
void wkcDebugPrintForceTerminatePeer(void) {}
void wkcDebugPrintfPeer(const char* fmt,...){ va_list v;va_start(v,fmt);vprintf(fmt,v);va_end(v); }
void wkcDebugPutsPeer(const char* s)       { puts(s); }
int  wkcDebugGetBacktracePeer(void**,int)  { return 0; }
void wkcDebugPrintBacktracePeer(void**,int){}
void peerDebugPrintf(const char* fmt,...)  { va_list v;va_start(v,fmt);vprintf(fmt,v);va_end(v); }

// ── System ───────────────────────────────────────────────────────────────────

bool wkcSystemInitializePeer(void)  { return true; }
void wkcSystemFinalizePeer(void)    {}

static const unsigned short kPlatform[]={'W','i','i','U',0};
static const unsigned short kProduct[] ={'G','e','c','k','o',0};
static const unsigned short kVendor[]  ={'W','a','v','e','B','r','o','w','s','e','r',0};
static const unsigned short kLang[]    ={'e','n',0};
static const unsigned short kEmpty[]   ={0};
static const unsigned short kSubmit[]  ={'S','u','b','m','i','t',0};
static const unsigned short kReset[]   ={'R','e','s','e','t',0};
static const unsigned short kFile[]    ={'C','h','o','o','s','e',' ','F','i','l','e',0};

const unsigned short* wkcSystemGetNavigatorPlatformPeer(void)   { return kPlatform; }
const unsigned short* wkcSystemGetNavigatorProductPeer(void)    { return kProduct; }
const unsigned short* wkcSystemGetNavigatorProductSubPeer(void) { return kEmpty; }
const unsigned short* wkcSystemGetNavigatorVendorPeer(void)     { return kVendor; }
const unsigned short* wkcSystemGetNavigatorVendorSubPeer(void)  { return kEmpty; }
const unsigned short* wkcSystemGetLanguagePeer(void)            { return kLang; }
const unsigned short* wkcSystemGetButtonLabelSubmitPeer(void)   { return kSubmit; }
const unsigned short* wkcSystemGetButtonLabelResetPeer(void)    { return kReset; }
const unsigned short* wkcSystemGetButtonLabelFilePeer(void)     { return kFile; }
void wkcSystemSetNavigatorPlatformPeer(const unsigned short*)   {}
void wkcSystemSetNavigatorProductPeer(const unsigned short*)    {}
void wkcSystemSetNavigatorProductSubPeer(const unsigned short*) {}
void wkcSystemSetNavigatorVendorPeer(const unsigned short*)     {}
void wkcSystemSetNavigatorVendorSubPeer(const unsigned short*)  {}
void wkcSystemSetLanguagePeer(const unsigned short*)            {}
void wkcSystemSetButtonLabelSubmitPeer(const unsigned short*)   {}
void wkcSystemSetButtonLabelResetPeer(const unsigned short*)    {}
void wkcSystemSetButtonLabelFilePeer(const unsigned short*)     {}
bool wkcSystemSetSystemStringPeer(const unsigned char*,const unsigned char*){ return true; }
const unsigned char* wkcSystemGetSystemStringPeer(const unsigned char*)     { return nullptr; }

// ── SSL ──────────────────────────────────────────────────────────────────────

bool  wkcSSLInitializePeer(void)           { return true; }
void  wkcSSLFinalizePeer(void)             {}
void  wkcSSLForceTerminatePeer(void)       {}
void* wkcSSLRegisterRootCAPeer(const char*,int)  { return (void*)1; }
int   wkcSSLUnregisterRootCAPeer(void*)    { return 0; }
void  wkcSSLRootCADeleteAllPeer(void)      {}
void* wkcSSLRegisterCRLPeer(const char*,int)     { return (void*)1; }
int   wkcSSLUnregisterCRLPeer(void*)       { return 0; }
void  wkcSSLCRLDeleteAllPeer(void)         {}

FILE* wkcOsslCertfOpenPeer(const char* p,const char* m)        { return fopen(p,m); }
int   wkcOsslCertfClosePeer(FILE* f)                           { return fclose(f); }
size_t wkcOsslCertfReadPeer(void* b,size_t s,size_t n,FILE* f) { return fread(b,s,n,f); }
size_t wkcOsslCertfWritePeer(const void* b,size_t s,size_t n,FILE* f){ return fwrite(b,s,n,f); }
char*  wkcOsslCertfGetsPeer(char* s,int n,FILE* f)             { return fgets(s,n,f); }
size_t wkcOsslCertfFlushPeer(FILE* f)                          { return fflush(f); }
int    wkcOsslCertfSeekPeer(FILE* f,long o,int w)              { return fseek(f,o,w); }
long   wkcOsslCertfTellPeer(FILE* f)                           { return ftell(f); }
bool   wkcOsslCertfIsRegistPeer(void)                          { return false; }
bool   wkcOsslCRLIsRegistPeer(void)                            { return false; }

FILE*  wkcOsslRandFilefOpenPeer(const char* p,const char* m)         { return fopen(p,m); }
int    wkcOsslRandFilefClosePeer(FILE* f)                            { return fclose(f); }
int    wkcOsslRandFileStatPeer(const char* p,struct stat* s)         { return stat(p,s); }
size_t wkcOsslRandFilefReadPeer(void* b,size_t s,size_t n,FILE* f)  { return fread(b,s,n,f); }
size_t wkcOsslRandFilefWritePeer(const void* b,size_t s,size_t n,FILE* f){ return fwrite(b,s,n,f); }

int          wkcNetCheckCorrectIPAddressPeer(const char*) { return 1; }
unsigned int wkcRandomNumberPeer(void)            { return (unsigned int)rand(); }
int          wkcRandomNumbersPeer(unsigned char* b,int n){ for(int i=0;i<n;i++)b[i]=(unsigned char)rand();return n; }
unsigned int wkcRandomMaxPeer(void)               { return RAND_MAX; }

// ── PCAP ─────────────────────────────────────────────────────────────────────

bool wkcPCAPInitializePeer(const char*) { return true; }
void wkcPCAPFinalizePeer(void)          {}
void wkcPCAPForceTerminatePeer(void)    {}

// ── Pasteboard ───────────────────────────────────────────────────────────────

void wkcPasteboardCallbackSetPeer(const WKCPasteboardProcs*) {}
void wkcPasteboardClearPeer(void)                {}
void wkcPasteboardWriteHTMLPeer(const char*,int,const char*,int,const char*,int,bool){}
void wkcPasteboardWritePlainTextPeer(const char*,int) {}
void wkcPasteboardWriteURIPeer(const char*,int,const char*,int) {}
void wkcPasteboardWriteImagePeer(int,void*,int,void*,int,const WKCSize*) {}
int  wkcPasteboardReadHTMLPeer(char*,int)        { return 0; }
int  wkcPasteboardReadHTMLURIPeer(char*,int)     { return 0; }
int  wkcPasteboardReadPlainTextPeer(char*,int)   { return 0; }
bool wkcPasteboardIsFormatAvailablePeer(int)     { return false; }

// ── Misc ─────────────────────────────────────────────────────────────────────

const char* wkcGuessMIMETypeByContentPeer(const unsigned char*,size_t){ return "application/octet-stream"; }

// ── Font engine ──────────────────────────────────────────────────────────────
// fontPeerMalloc/fontPeerFree already typedef'd in wkcgpeer.h — do NOT redefine

bool wkcFontEngineInitializePeer(void*,int,fontPeerMalloc,fontPeerFree,bool){ return true; }
void wkcFontEngineFinalizePeer(void)       {}
void wkcFontEngineForceTerminatePeer(void) {}
bool wkcFontEngineCanSuspendPeer(void)     { return false; }
bool wkcFontEngineSupportsNonSpacingMarksPeer(void){ return false; }
bool wkcFontEngineCanSupportPeer(const unsigned char*,unsigned int){ return true; }
bool wkcFontEngineCanSupportByFormatNamePeer(const unsigned char*,unsigned int){ return true; }
int  wkcFontEngineRegisterSystemFontPeer(int,const unsigned char*,unsigned int){ return 0; }
int  wkcFontEngineRegisterFontPeer(int,const unsigned char*,unsigned int)      { return 1; }
void wkcFontEngineUnregisterFontPeer(int)  {}
void wkcFontEngineUnregisterFontsPeer()    {}
bool wkcFontEngineGetFamilyNamePeer(int,char* out,int mx){ strncpy(out,"Roboto",mx-1);return true; }
bool wkcFontEngineSetPrimaryFontPeer(int)  { return true; }
int  wkcFontEngineHeapSizePeer(void)       { return 0; }
bool wkcFontEngineSetFontScalePeer(int,float){ return true; }

void* wkcFontNewPeer(int size,int,bool,bool,bool,int,const char*){
    auto* f=(WKCFont*)malloc(sizeof(WKCFont));
    if (!f) return nullptr;
    f->size=size;
    f->f=TTF_OpenFontRW(SDL_RWFromConstMem(font_data,font_data_len),0,size);
    return f;
}
void* wkcFontNewCopyPeer(void* p){
    return p ? wkcFontNewPeer(((WKCFont*)p)->size,0,false,false,false,0,nullptr) : nullptr;
}
void wkcFontDeletePeer(void* p){ if(p){auto*f=(WKCFont*)p;TTF_CloseFont(f->f);free(f);} }
int  wkcFontSizeOfFontInstancePeer(void*)    { return sizeof(WKCFont); }
bool wkcFontCanScalePeer(void*)              { return true; }
bool wkcFontCanSupportDrawComplexPeer(void*) { return false; }
bool wkcFontCanTransformPeer(void*)          { return false; }
bool wkcFontIsEqualPeer(void* a,void* b){
    if(!a||!b) return a==b;
    return ((WKCFont*)a)->size==((WKCFont*)b)->size;
}
void wkcFontSetMatrixPeer(void*,float,float,float,float,float,float){}
int  wkcFontGetSizePeer(void* p)     { return p?((WKCFont*)p)->size:0; }
int  wkcFontGetAscentPeer(void* p)   { return (p&&((WKCFont*)p)->f)?TTF_FontAscent(((WKCFont*)p)->f):0; }
int  wkcFontGetDescentPeer(void* p)  { return (p&&((WKCFont*)p)->f)?-TTF_FontDescent(((WKCFont*)p)->f):0; }
int  wkcFontGetLineSpacingPeer(void* p){ return (p&&((WKCFont*)p)->f)?TTF_FontLineSkip(((WKCFont*)p)->f):0; }
int  wkcFontGetLineGapPeer(void* p)  {
    if(!p||!((WKCFont*)p)->f) return 0;
    return TTF_FontLineSkip(((WKCFont*)p)->f)-TTF_FontHeight(((WKCFont*)p)->f);
}
int wkcFontGetXHeightPeer(void* p)        { int w,h=0;if(p&&((WKCFont*)p)->f)TTF_SizeUTF8(((WKCFont*)p)->f,"x",&w,&h);return h; }
int wkcFontGetAverageCharWidthPeer(void* p){ int w=0,h;if(p&&((WKCFont*)p)->f)TTF_SizeUTF8(((WKCFont*)p)->f,"M",&w,&h);return w; }
int wkcFontGetMaxCharWidthPeer(void* p)   { int w=0,h;if(p&&((WKCFont*)p)->f)TTF_SizeUTF8(((WKCFont*)p)->f,"W",&w,&h);return w; }
bool wkcFontIsFixedFontPeer(void*)        { return false; }

int wkcFontGetTextWidthPeer(void* p,int,const unsigned short* str,int len,int* clip){
    if(!p||!str||!((WKCFont*)p)->f) return 0;
    char buf[512]; u16_to_u8(str,len,buf,512);
    int w=0,h; TTF_SizeUTF8(((WKCFont*)p)->f,buf,&w,&h);
    if(clip) *clip=w;
    return w;
}

// Exact signature from wkcgpeer.h line 460
int wkcFontDrawTextPeer(void* in_font, int /*flags*/,
                        const unsigned short* in_str, int in_strlen,
                        void* in_offscreen, const WKCSize* /*in_offscreensize*/,
                        int in_rowbytes, int /*in_fmt*/,
                        const WKCFloatRect* in_textbox, const WKCFloatRect* /*in_clip*/,
                        int /*in_drawingmode*/, unsigned int in_color, unsigned int /*strokecolor*/,
                        int /*strokestyle*/, float /*strokethickness*/, float /*zoomlevel*/)
{
    if (!in_font||!in_str||!in_offscreen||!((WKCFont*)in_font)->f) return 0;
    char buf[512]; u16_to_u8(in_str,in_strlen,buf,512);
    SDL_Color sc={(uint8_t)((in_color>>16)&0xFF),(uint8_t)((in_color>>8)&0xFF),(uint8_t)(in_color&0xFF),0xFF};
    SDL_Surface* surf=TTF_RenderUTF8_Blended(((WKCFont*)in_font)->f,buf,sc);
    if (!surf) return 0;
    int dx=in_textbox?(int)in_textbox->fPoint.fX:0;
    int dy=in_textbox?(int)in_textbox->fPoint.fY:0;
    SDL_LockSurface(surf);
    for (int y=0;y<surf->h;y++) {
        int by=dy+y; if(by<0||by>=s_h) continue;
        auto* sr=(uint8_t*)surf->pixels+y*surf->pitch;
        auto* dr=(uint8_t*)in_offscreen+by*in_rowbytes+dx*4;
        for (int x=0;x<surf->w;x++) {
            auto* s=sr+x*4; auto* d=dr+x*4;
            uint8_t a=s[3];
            d[0]=(uint8_t)(s[0]*a/255+d[0]*(255-a)/255);
            d[1]=(uint8_t)(s[1]*a/255+d[1]*(255-a)/255);
            d[2]=(uint8_t)(s[2]*a/255+d[2]*(255-a)/255);
            d[3]=255;
        }
    }
    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
    return in_strlen;
}

// ── Stock image ───────────────────────────────────────────────────────────────

const unsigned char* wkcStockImageGetBitmapPeer(int)               { return k_white; }
void wkcStockImageGetSizePeer(int,unsigned int* w,unsigned int* h)  { *w=1;*h=1; }
const WKCPoint* wkcStockImageGetLayoutPointsPeer(int)              { return nullptr; }
unsigned int wkcStockImageGetSkinColorPeer(int)                    { return 0xFFFFFFFF; }
float wkcStockImageGetSystemFontSizePeer(int)                      { return 13.f; }
const char* wkcStockImageGetSystemFontFamilyNamePeer(int)          { return "Roboto"; }
void wkcStockImageRegisterSkinPeer(const wkcSkin*)                 {}
const char* wkcStockImageGetDefaultStyleSheetPeer(void)            { return nullptr; }
const char* wkcStockImageGetQuirksStyleSheetPeer(void)             { return nullptr; }
const unsigned char* wkcStockImageGetPlatformResourceImagePeer(const char*,unsigned int* w,unsigned int* h)
{ *w=1;*h=1; return k_white; }

// ── Offscreen ────────────────────────────────────────────────────────────────
// WKCRect: { WKCPoint fPoint{fX,fY}; WKCSize fSize{fWidth,fHeight}; }

void* wkcOffscreenNewPeer(int,void* bmp,int rb,const WKCSize* sz){
    auto* o=(WKCOff*)calloc(1,sizeof(WKCOff));
    if(!o) return nullptr;
    o->w=sz?sz->fWidth:s_w; o->h=sz?sz->fHeight:s_h; o->rb=rb?rb:o->w*4;
    if(bmp){o->bmp=(unsigned char*)bmp;o->owned=false;}
    else{o->bmp=(unsigned char*)malloc(o->rb*o->h);o->owned=true;if(o->bmp)memset(o->bmp,0xFF,o->rb*o->h);}
    return o;
}
bool wkcOffscreenIsAcceleratedPeer(void*) { return false; }
void* wkcTextureNewPeer(const WKCSize*)   { return nullptr; }
void wkcTextureSetImagePeer(void*,void*)  {}
void wkcTextureClearImagePeer(void*,void*){}
void wkcOffscreenDeletePeer(void* p)      { if(p){auto*o=(WKCOff*)p;if(o->owned)free(o->bmp);free(o);} }
void wkcTextureDeletePeer(void*)          {}
void wkcOffscreenForceTerminatePeer(void) {}
void wkcOffscreenResizePeer(void* p,const WKCSize* sz){
    if(!p||!sz) return;
    auto*o=(WKCOff*)p;
    if(o->owned)free(o->bmp);
    o->w=sz->fWidth;o->h=sz->fHeight;o->rb=o->w*4;
    o->bmp=(unsigned char*)malloc(o->rb*o->h);o->owned=true;
    if(o->bmp)memset(o->bmp,0xFF,o->rb*o->h);
}
void wkcOffscreenGetSizePeer(void* p,WKCSize* sz){
    if(p&&sz){sz->fWidth=((WKCOff*)p)->w;sz->fHeight=((WKCOff*)p)->h;}
}
void wkcOffscreenScrollPeer(void* p,const WKCRect* r,const WKCSize* d){
    // WKCRect uses fPoint.fX/fY and fSize.fWidth/fHeight
    if(!p||!r||!d) return;
    auto*o=(WKCOff*)p;
    int rx=r->fPoint.fX, ry=r->fPoint.fY, rh=r->fSize.fHeight;
    int dx=d->fWidth,    dy=d->fHeight;
    memmove(o->bmp+(ry+dy)*o->rb+(rx+dx)*4,
            o->bmp+ry*o->rb+rx*4,
            (size_t)(rh-abs(dy))*o->rb);
}
void wkcOffscreenSetOpticalZoomPeer(void*,float,const WKCFloatPoint*){}
void wkcOffscreenSetUseInterpolationForImagePeer(void*,bool){}
void wkcOffscreenSetUseAntiAliasForPolygonPeer(void*,bool)  {}
void* wkcOffscreenBitmapPeer(void* p,int* rb){
    if(!p) return nullptr;
    auto*o=(WKCOff*)p;
    if(rb) *rb=o->rb;
    if(s_pixels&&o->w==s_w&&o->h==s_h){
        memcpy(s_pixels,o->bmp,(size_t)o->rb*o->h);
        s_dirty=true;
    }
    return o->bmp;
}

// =============================================================================
} // extern "C"
// =============================================================================

bool webkit_engine_init(int w, int h)
{
    s_w=w; s_h=h;
    s_pixels=(unsigned char*)malloc(w*h*4);
    if(!s_pixels) return false;
    memset(s_pixels,0xFF,w*h*4);

    wkcTimerInitializePeer(nullptr);
    wkcThreadInitializePeer();
    wkcMemoryInitializePeer(nullptr,nullptr,nullptr);
    wkcFileInitializePeer();
    wkcDebugPrintInitializePeer();
    wkcSystemInitializePeer();
    wkcSSLInitializePeer();
    wkcFontEngineInitializePeer(nullptr,0,nullptr,nullptr,false);

    // TODO: WKCWebKit::initialize + WKCWebView::create when available in .a
    s_available=false;
    return true;
}

void webkit_engine_set_renderer(SDL_Renderer* ren)
{
    s_renderer=ren;
    if(s_renderer&&!s_texture)
        s_texture=SDL_CreateTexture(s_renderer,SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,s_w,s_h);
}

void webkit_engine_navigate(const char* url)
{
    if(!url) return;
    strncpy(s_url,url,sizeof(s_url)-1);
    if(s_available){
        // TODO: s_webview->loadURL(url);
        return;
    }
    // Placeholder: light grey with blue top stripe
    if(s_pixels){
        memset(s_pixels,0xF4,s_w*s_h*4);
        for(int x=0;x<s_w;x++){
            uint8_t*p=s_pixels+x*4;
            p[0]=0xFF;p[1]=0x42;p[2]=0x85;p[3]=0xF4;
        }
        s_dirty=true;
    }
}

void webkit_engine_tick(void)
{
    if(!s_timer_init) return;
    unsigned int now=now_ms();
    OSLockMutex(&s_timer_mtx);
    for(WKCTimer*t=s_timers;t;t=t->next){
        if(!t->active||now<t->fire_at) continue;
        t->active=false;
        auto cb=t->proc; auto data=t->data; auto iv=t->interval;
        if(iv){t->fire_at=now+iv;t->active=true;}
        OSUnlockMutex(&s_timer_mtx);
        if(cb) cb(data);
        OSLockMutex(&s_timer_mtx);
    }
    OSUnlockMutex(&s_timer_mtx);
}

void webkit_engine_draw(int cx,int cy,int cw,int ch)
{
    if(!s_renderer||!s_texture||!s_pixels) return;
    if(s_dirty||s_available){
        SDL_UpdateTexture(s_texture,nullptr,s_pixels,s_w*4);
        s_dirty=false;
    }
    SDL_Rect src={0,0,s_w,s_h};
    SDL_Rect dst={cx,cy,cw,ch};
    SDL_RenderCopy(s_renderer,s_texture,&src,&dst);
}

void webkit_engine_input_text(const char* u){ (void)u; }
void webkit_engine_scroll(int x,int y){ (void)x;(void)y; }
void webkit_engine_touch_down(int x,int y){ (void)x;(void)y; }
void webkit_engine_touch_up(int x,int y){ (void)x;(void)y; }
bool webkit_engine_available(void){ return s_available; }

void webkit_engine_shutdown(void)
{
    if(s_texture){SDL_DestroyTexture(s_texture);s_texture=nullptr;}
    wkcSSLFinalizePeer();
    wkcFontEngineFinalizePeer();
    wkcSystemFinalizePeer();
    wkcDebugPrintFinalizePeer();
    wkcFileFinalizePeer();
    wkcMemoryFinalizePeer();
    wkcThreadFinalizePeer();
    wkcTimerFinalizePeer();
    free(s_pixels);s_pixels=nullptr;
    s_available=false;
}
