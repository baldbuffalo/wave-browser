// webkit_engine.cpp — WKC peer implementations + SDL integration
// WKC API headers come from the matching WebKit-WiiU full-engine release artifact.

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
#include <wkc/WKCWebView.h>
#include <wkc/WKCClientBuilders.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
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
static WKC::WKCWebView* s_webview = nullptr;
static bool s_webkit_initialized = false;

namespace {
class WaveMemoryHandler final : public WKC::WKCMemoryEventHandler {
public:
    bool checkMemoryAvailability(unsigned int, bool) override { return true; }
    bool checkMemoryAllocatable(unsigned int, AllocationReason) override { return true; }
    void* notifyMemoryExhaust(unsigned int, unsigned int& allocated) override
    {
        allocated = 0;
        return nullptr;
    }
    void notifyMemoryAllocationError(unsigned int, AllocationReason) override {}
    void notifyCrash(const char* file, int line, const char* function, const char* assertion) override
    {
        fprintf(stderr, "WebKit-WiiU crash: %s:%d %s (%s)\n",
                file ? file : "?", line, function ? function : "?",
                assertion ? assertion : "?");
    }
    void notifyStackOverflow(bool, unsigned int, unsigned int, unsigned int,
                             void*, void*, void*, const char*, int, const char*) override {}
};

class WaveTimerHandler final : public WKC::WKCTimerEventHandler {
public:
    bool requestWakeUp(unsigned int, void*) override { return true; }
};

// The modern WKCWebView constructs its WebCore clients internally. These
// legacy WKC API callbacks are not used by the current port's page pipeline.
class WaveClientBuilders final : public WKC::WKCClientBuilders {
public:
    WKC::ChromeClientIf* createChromeClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteChromeClient(WKC::ChromeClientIf*) override {}
    WKC::ContextMenuClientIf* createContextMenuClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteContextMenuClient(WKC::ContextMenuClientIf*) override {}
    WKC::DragClientIf* createDragClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteDragClient(WKC::DragClientIf*) override {}
    WKC::EditorClientIf* createEditorClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteEditorClient(WKC::EditorClientIf*) override {}
    WKC::FrameLoaderClientIf* createFrameLoaderClient(WKC::WKCWebFrame*) override { return nullptr; }
    void deleteFrameLoaderClient(WKC::FrameLoaderClientIf*) override {}
    WKC::InspectorClientIf* createInspectorClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteInspectorClient(WKC::InspectorClientIf*) override {}
    WKC::DropDownListClientIf* createDropDownListClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteDropDownListClient(WKC::DropDownListClientIf*) override {}
    WKC::GeolocationClientIf* createGeolocationClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteGeolocationClient(WKC::GeolocationClientIf*) override {}
    WKC::DeviceMotionClientIf* createDeviceMotionClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteDeviceMotionClient(WKC::DeviceMotionClientIf*) override {}
    WKC::DeviceOrientationClientIf* createDeviceOrientationClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteDeviceOrientationClient(WKC::DeviceOrientationClientIf*) override {}
    WKC::BackForwardClientIf* createBackForwardClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteBackForwardClient(WKC::BackForwardClientIf*) override {}
    WKC::BatteryClientIf* createBatteryClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteBatteryClient(WKC::BatteryClientIf*) override {}
};
static WaveMemoryHandler s_memory_handler;
static WaveTimerHandler s_timer_handler;
static WaveClientBuilders s_client_builders;
}

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
    const char* argv[1];
};

// Store the thread context in the new thread's argv. Thread-specific storage
// belongs to the running thread, so setting it in the creator is incorrect.
#define WKC_THREAD_SLOT ((OSThreadSpecificID)0)

static int thread_trampoline(int argc, const char** argv) {
    if (argc < 1 || !argv || !argv[0]) return -1;
    WKCThread* t = (WKCThread*)(uintptr_t)strtoull(argv[0], nullptr, 16);
    if (!t) return -1;
    OSSetThreadSpecific(WKC_THREAD_SLOT, t);
    if (t->fn) t->result = t->fn(t->arg);
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
void wkcTimerCancelPeer(void* p) {
    if (!p || !s_timer_init) return;
    OSLockMutex(&s_timer_mtx);
    ((WKCTimer*)p)->active = false;
    OSUnlockMutex(&s_timer_mtx);
}
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
    // Pass the context as a hexadecimal string to avoid relying on the
    // creator's thread-specific storage being inherited by the worker.
    char context[2 * sizeof(uintptr_t) + 1];
    snprintf(context, sizeof(context), "%llx",
             (unsigned long long)(uintptr_t)t);
    t->argv[0] = strdup(context);
    if (!t->argv[0]) { free(t->stack); free(t); return nullptr; }
    if (!OSCreateThread(&t->ost, thread_trampoline, 1, t->argv,
                        t->stack+64*1024, 64*1024, 16, OS_THREAD_ATTRIB_AFFINITY_ANY)) {
        free((void*)t->argv[0]);
        free(t->stack);
        free(t);
        return nullptr;
    }
    OSResumeThread(&t->ost);
    return t;
}
void* wkcThreadCreateForMultiCorePeer(void*(*fn)(void*), void* a) { return wkcThreadCreatePeer(fn,a); }

int wkcThreadJoinPeer(void* p, void** res) {
    if (!p) return -1;
    auto* t=(WKCThread*)p;
    OSJoinThread(&t->ost, nullptr);
    if (res) *res=t->result;
    free((void*)t->argv[0]);
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
    if (!p || !out || mx <= 0) return 0;
    const char* s=strrchr(p,'/'); s=s?s+1:p;
    const size_t n=strlen(s);
    const size_t copy=n<(size_t)(mx-1)?n:(size_t)(mx-1);
    if (copy) memcpy(out,s,copy);
    out[copy]='\0';
    return (int)copy;
}
int wkcFilePathByAppendingComponentPeer(const char* p,const char* c,char* out,int mx){
    if (!p || !c || !out || mx <= 0) return 0;
    const int written=snprintf(out,(size_t)mx,"%s/%s",p,c);
    if (written < 0) { out[0]='\0'; return 0; }
    return written < mx ? written : mx-1;
}
int wkcFileHomeDirectoryPathPeer(char* out,int mx){
    static const char home[]="fs:/vol/external01/wiiu/apps/WaveBrowser";
    if (!out || mx <= 0) return 0;
    const size_t n=strlen(home);
    const size_t copy=n<(size_t)(mx-1)?n:(size_t)(mx-1);
    if (copy) memcpy(out,home,copy);
    out[copy]='\0';
    return (int)copy;
}
int  wkcFileListDirectoryPeer(const char*,const char*,char**,int,int){ return 0; }
int  wkcFileDirectoryNamePeer(const char* p,char* out,int mx){
    if (!p || !out || mx <= 0) return 0;
    const char* s=strrchr(p,'/');
    if(!s){out[0]='.'; if(mx>1) out[1]='\0'; return 1;}
    size_t n=(size_t)(s-p);
    if(n>(size_t)(mx-1)) n=(size_t)(mx-1);
    if(n) memcpy(out,p,n);
    out[n]='\0';
    return (int)n;
}
void* wkcFileOpenTemporaryFilePeer(const char*,char* out,int mx){
    strncpy(out,"fs:/vol/external01/wiiu/apps/WaveBrowser/tmp",mx-1);return nullptr;
}
// WKC expects an opaque directory handle and a filename copied into the
// caller's buffer on each read. Use the platform C directory API rather than
// reporting an empty directory for every request.
void* wkcFileOpenDirectoryPeer(int, const char* path, const char*)
{
    if (!path || !path[0])
        return nullptr;
    return (void*)opendir(path);
}

void wkcFileCloseDirectoryPeer(void* handle)
{
    if (handle)
        closedir((DIR*)handle);
}

int wkcFileReadDirectoryPeer(void* handle, char* name, int maxLength)
{
    if (!handle || !name || maxLength <= 0)
        return -1;

    name[0] = '\\0';
    DIR* dir = (DIR*)handle;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip the current and parent-directory entries; WebCore callers
        // should see real children, not traversal pseudo-entries.
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        const size_t length = strlen(entry->d_name);
        if (length >= (size_t)maxLength) {
            // Do not return a truncated name as if it were complete.
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(name, entry->d_name, length + 1);
        return (int)length;
    }

    // A zero return indicates end-of-directory; distinguish an actual
    // directory read error with -1.
    return (errno == 0) ? 0 : -1;
}

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
int          wkcRandomNumbersPeer(unsigned char* b,int n){
    // Reject invalid requests instead of dereferencing a null buffer or
    // reporting a negative byte count as a successful result.
    if (n < 0 || (n > 0 && !b)) return -1;
    for (int i = 0; i < n; ++i) b[i] = (unsigned char)rand();
    return n;
}
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
    const int width = sz ? sz->fWidth : s_w;
    const int height = sz ? sz->fHeight : s_h;
    if (width <= 0 || height <= 0 || width > INT_MAX / 4)
        return nullptr;
    const int rowBytes = rb ? rb : width * 4;
    if (rowBytes < width * 4 || (size_t)height > SIZE_MAX / (size_t)rowBytes)
        return nullptr;

    auto* o = (WKCOff*)calloc(1, sizeof(WKCOff));
    if (!o)
        return nullptr;
    o->w = width;
    o->h = height;
    o->rb = rowBytes;
    if (bmp) {
        o->bmp = (unsigned char*)bmp;
        o->owned = false;
    } else {
        o->bmp = (unsigned char*)malloc((size_t)o->rb * (size_t)o->h);
        if (!o->bmp) {
            free(o);
            return nullptr;
        }
        o->owned = true;
        memset(o->bmp, 0xFF, (size_t)o->rb * (size_t)o->h);
    }
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
    if (!p || !sz || sz->fWidth <= 0 || sz->fHeight <= 0 ||
        sz->fWidth > INT_MAX / 4)
        return;
    auto* o = (WKCOff*)p;
    const int newRowBytes = sz->fWidth * 4;
    if ((size_t)sz->fHeight > SIZE_MAX / (size_t)newRowBytes)
        return;

    // Allocate before releasing the old bitmap. A failed resize must leave the
    // existing framebuffer usable instead of turning it into a null pointer.
    unsigned char* newBitmap = (unsigned char*)malloc((size_t)newRowBytes * (size_t)sz->fHeight);
    if (!newBitmap)
        return;
    memset(newBitmap, 0xFF, (size_t)newRowBytes * (size_t)sz->fHeight);

    if (o->owned)
        free(o->bmp);
    o->bmp = newBitmap;
    o->owned = true;
    o->w = sz->fWidth;
    o->h = sz->fHeight;
    o->rb = newRowBytes;
}
void wkcOffscreenGetSizePeer(void* p,WKCSize* sz){
    if(p&&sz){sz->fWidth=((WKCOff*)p)->w;sz->fHeight=((WKCOff*)p)->h;}
}
void wkcOffscreenScrollPeer(void* p,const WKCRect* r,const WKCSize* d){
    // Scroll only the requested rectangle and clip both source and destination
    // to the bitmap. Use 64-bit intermediates because caller-supplied rectangle
    // and delta values can overflow signed int before clipping.
    if (!p || !r || !d) return;
    auto* o = (WKCOff*)p;
    if (!o->bmp || o->w <= 0 || o->h <= 0 ||
        o->w > INT_MAX / 4 || o->rb < o->w * 4)
        return;

    const int64_t rx = r->fPoint.fX, ry = r->fPoint.fY;
    const int64_t rw = r->fSize.fWidth, rh = r->fSize.fHeight;
    const int64_t dx = d->fWidth, dy = d->fHeight;
    if (rw <= 0 || rh <= 0 || (!dx && !dy)) return;

    // Source pixels must be in bounds, as must their translated destination.
    int64_t sx0 = rx > 0 ? rx : 0;
    int64_t sy0 = ry > 0 ? ry : 0;
    int64_t sx1 = rx + rw;
    int64_t sy1 = ry + rh;
    if (sx0 < -dx) sx0 = -dx;
    if (sy0 < -dy) sy0 = -dy;
    if (sx1 > o->w) sx1 = o->w;
    if (sy1 > o->h) sy1 = o->h;
    if (sx1 > (int64_t)o->w - dx) sx1 = (int64_t)o->w - dx;
    if (sy1 > (int64_t)o->h - dy) sy1 = (int64_t)o->h - dy;
    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    if (sx0 >= sx1 || sy0 >= sy1) return;

    const size_t rowBytes = (size_t)(sx1 - sx0) * 4;
    if (dy > 0) {
        for (int64_t y = sy1 - 1; y >= sy0; --y) {
            const size_t src = (size_t)y * (size_t)o->rb + (size_t)sx0 * 4;
            const size_t dst = (size_t)(y + dy) * (size_t)o->rb + (size_t)(sx0 + dx) * 4;
            memmove(o->bmp + dst, o->bmp + src, rowBytes);
        }
    } else {
        for (int64_t y = sy0; y < sy1; ++y) {
            const size_t src = (size_t)y * (size_t)o->rb + (size_t)sx0 * 4;
            const size_t dst = (size_t)(y + dy) * (size_t)o->rb + (size_t)(sx0 + dx) * 4;
            memmove(o->bmp + dst, o->bmp + src, rowBytes);
        }
    }
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
    if (w <= 0 || h <= 0)
        return false;
    if (s_webview || s_webkit_initialized || s_pixels)
        webkit_engine_shutdown();

    // Validate all size arithmetic before allocating or clearing the page
    // framebuffer; signed w*h*4 can overflow even when each dimension is > 0.
    if ((size_t)w > SIZE_MAX / 4 / (size_t)h)
        return false;
    const size_t pixelBytes = (size_t)w * (size_t)h * 4;
    s_pixels = (unsigned char*)malloc(pixelBytes);
    if (!s_pixels)
        return false;
    memset(s_pixels, 0xFF, pixelBytes);

    wkcTimerInitializePeer(nullptr);
    wkcThreadInitializePeer();
    wkcMemoryInitializePeer(nullptr,nullptr,nullptr);
    wkcFileInitializePeer();
    wkcDebugPrintInitializePeer();
    wkcSystemInitializePeer();
    wkcSSLInitializePeer();
    wkcFontEngineInitializePeer(nullptr,0,nullptr,nullptr,false);

    // Initialize the actual WebKit runtime from the downloaded full-engine
    // archive, then create a WebView backed by the WKC platform peers above.
    if (!WKC::WKCWebKitInitialize(nullptr, 0, nullptr, 0,
                                  s_memory_handler, s_timer_handler)) {
        fprintf(stderr, "WebKit-WiiU initialization failed\n");
        wkcSSLFinalizePeer();
        wkcFontEngineFinalizePeer();
        wkcSystemFinalizePeer();
        wkcDebugPrintFinalizePeer();
        wkcFileFinalizePeer();
        wkcMemoryFinalizePeer();
        wkcThreadFinalizePeer();
        wkcTimerFinalizePeer();
        free(s_pixels);
        s_pixels = nullptr;
        return false;
    }
    s_webkit_initialized = true;
    s_webview = WKC::WKCWebView::create(s_client_builders);
    if (!s_webview) {
        fprintf(stderr, "WebKit-WiiU could not create WKCWebView\n");
        WKC::WKCWebKitFinalize();
        s_webkit_initialized = false;
        wkcSSLFinalizePeer();
        wkcFontEngineFinalizePeer();
        wkcSystemFinalizePeer();
        wkcDebugPrintFinalizePeer();
        wkcFileFinalizePeer();
        wkcMemoryFinalizePeer();
        wkcThreadFinalizePeer();
        wkcTimerFinalizePeer();
        free(s_pixels);
        s_pixels = nullptr;
        return false;
    }

    WKCSize offscreen = {};
    offscreen.fWidth = w;
    offscreen.fHeight = h;
    if (!s_webview->setOffscreen(WKC::EOffscreenFormatARGB8888, s_pixels, w * 4,
                                 offscreen, offscreen, true, &offscreen, true)) {
        fprintf(stderr, "WebKit-WiiU could not attach the page framebuffer\n");
        WKC::WKCWebView::deleteWKCWebView(s_webview);
        s_webview = nullptr;
        WKC::WKCWebKitFinalize();
        s_webkit_initialized = false;
        wkcSSLFinalizePeer();
        wkcFontEngineFinalizePeer();
        wkcSystemFinalizePeer();
        wkcDebugPrintFinalizePeer();
        wkcFileFinalizePeer();
        wkcMemoryFinalizePeer();
        wkcThreadFinalizePeer();
        wkcTimerFinalizePeer();
        free(s_pixels);
        s_pixels = nullptr;
        return false;
    }
    s_available = true;

    // Wave Browser may set its startup URL before the engine has been created.
    // Replay that pending navigation now that WKCWebView is ready; otherwise
    // the early Google request would only paint the fallback surface.
    if (s_url[0] != '\0') {
        if (!s_webview->loadURLOrSearch(s_url)) {
            fprintf(stderr, "WebKit-WiiU rejected pending startup URL: %s\n", s_url);
        } else {
            s_dirty = true;
        }
    }
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
    if (s_available && s_webview) {
        if (s_webview->loadURLOrSearch(url)) {
            s_dirty = true;
            return;
        }
        fprintf(stderr, "WebKit-WiiU rejected navigation URL: %s\n", url);
    }
    // Keep a visible failure surface rather than pretending the placeholder is
    // a loaded page; navigation is retried when the engine becomes available.
    if (s_pixels) {
        memset(s_pixels, 0xF4, (size_t)s_w * s_h * 4);
        for (int x = 0; x < s_w; ++x) {
            uint8_t* p = s_pixels + x * 4;
            p[0] = 0xFF; p[1] = 0x42; p[2] = 0x85; p[3] = 0xF4;
        }
        s_dirty = true;
    }
}

void webkit_engine_tick(void)
{
    if (s_available && s_webview) {
        WKC::WKCWebKitWakeUp(nullptr);
        s_webview->notifyServiceScriptedAnimations();
    }
    if (!s_timer_init) return;

    struct TimerCall {
        wkcTimeoutProc proc;
        void* data;
    };

    const unsigned int now = now_ms();
    OSLockMutex(&s_timer_mtx);

    // Count due callbacks first so the snapshot can be allocated before any
    // one-shot timer is deactivated. Never retain timer-list pointers while
    // callbacks run: a callback is allowed to delete itself or another timer.
    size_t dueCount = 0;
    for (WKCTimer* t = s_timers; t; t = t->next) {
        if (t->active && now >= t->fire_at && t->proc)
            ++dueCount;
    }

    TimerCall* calls = dueCount
        ? static_cast<TimerCall*>(malloc(dueCount * sizeof(TimerCall)))
        : nullptr;
    if (dueCount && !calls) {
        OSUnlockMutex(&s_timer_mtx);
        return;
    }

    size_t callCount = 0;
    for (WKCTimer* t = s_timers; t; t = t->next) {
        if (!t->active || now < t->fire_at)
            continue;

        if (t->proc) {
            calls[callCount].proc = t->proc;
            calls[callCount].data = t->data;
            ++callCount;
        }

        if (t->interval) {
            t->fire_at = now + t->interval;
            t->active = true;
        } else {
            t->active = false;
        }
    }
    OSUnlockMutex(&s_timer_mtx);

    // The snapshot contains only callback/data values, so callbacks can
    // safely start, cancel, or delete timers without invalidating iteration.
    for (size_t i = 0; i < callCount; ++i)
        calls[i].proc(calls[i].data);
    free(calls);
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

void webkit_engine_input_text(const char* u)
{
    if (!s_available || !s_webview || !u) return;
    for (const unsigned char* p = (const unsigned char*)u; *p; ++p)
        s_webview->notifyKeyChar(*p);
}
void webkit_engine_scroll(int x,int y)
{
    if (s_available && s_webview) s_webview->notifyMouseWheel(WKCPoint{x, y}, WKCSize{x, y}, WKC::EModifierNone);
}
void webkit_engine_touch_down(int x,int y)
{
    if (s_available && s_webview) s_webview->notifyMouseDown(WKCPoint{x, y}, WKC::EMouseButtonLeft, WKC::EModifierNone);
}
void webkit_engine_touch_up(int x,int y)
{
    if (s_available && s_webview) s_webview->notifyMouseUp(WKCPoint{x, y}, WKC::EMouseButtonLeft, WKC::EModifierNone);
}
bool webkit_engine_available(void){ return s_available; }

void webkit_engine_shutdown(void)
{
    if(s_texture){SDL_DestroyTexture(s_texture);s_texture=nullptr;}
    if (s_webview) {
        WKC::WKCWebView::deleteWKCWebView(s_webview);
        s_webview = nullptr;
    }
    if (s_webkit_initialized) {
        WKC::WKCWebKitFinalize();
        s_webkit_initialized = false;
    }
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

void webkit_engine_key_down(int keycode)
{
    if (!s_available || !s_webview) return;
    s_webview->notifyKeyPress((WKC::Key)keycode, WKC::EModifierNone);
}

void webkit_engine_key_up(int keycode)
{
    if (!s_available || !s_webview) return;
    s_webview->notifyKeyRelease((WKC::Key)keycode, WKC::EModifierNone);
}
