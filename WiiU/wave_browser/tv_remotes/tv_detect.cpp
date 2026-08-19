#include "tv_detect.h"
#include "model_registry.h"
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/mutex.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#if __has_include(<acp/acp.h>)
#  include <acp/acp.h>
#  define HAVE_ACP 1
#else
#  define HAVE_ACP 0
#endif

#define CEC_OPCODE_GIVE_OSD_NAME      0x46
#define CEC_OPCODE_SET_OSD_NAME       0x47
#define CEC_OPCODE_GIVE_DEVICE_VENDOR 0x8C
#define CEC_OPCODE_DEVICE_VENDOR_ID   0x87

static OSMutex        s_mutex;
static bool           s_mutex_init = false;
static volatile bool  s_running    = false;
static volatile bool  s_done       = false;
static TVDetectResult s_result     = {};
static OSThread       s_thread;
static uint8_t        s_stack[8192];

static bool cec_send(uint8_t dst, uint8_t opcode)
{
#if HAVE_ACP
    uint8_t msg[2] = { (uint8_t)((4<<4)|(dst&0xF)), opcode };
    return ACPSendCECCommand(msg, 2) == 0;
#else
    (void)dst; (void)opcode; return false;
#endif
}

static int cec_recv(uint8_t* buf, int maxlen, uint32_t timeout_ms)
{
#if HAVE_ACP
    return ACPReceiveCECCommand(buf, maxlen, timeout_ms);
#else
    (void)buf; (void)maxlen; (void)timeout_ms; return 0;
#endif
}

static int detect_thread(int, const char**)
{
    TVDetectResult res = {};
    uint8_t buf[32]; int n;

    if (cec_send(0, CEC_OPCODE_GIVE_DEVICE_VENDOR)) {
        OSSleepTicks(OSMillisecondsToTicks(300));
        n = cec_recv(buf, sizeof(buf), 400);
        if (n >= 5 && buf[1] == CEC_OPCODE_DEVICE_VENDOR_ID)
            res.cec_vendor = ((uint32_t)buf[2]<<16)|((uint32_t)buf[3]<<8)|(uint32_t)buf[4];
    }

    if (cec_send(0, CEC_OPCODE_GIVE_OSD_NAME)) {
        OSSleepTicks(OSMillisecondsToTicks(300));
        n = cec_recv(buf, sizeof(buf), 400);
        if (n >= 2 && buf[1] == CEC_OPCODE_SET_OSD_NAME) {
            int len = n-2; if (len>15) len=15;
            memcpy(res.osd_name, buf+2, (size_t)len);
            res.osd_name[len] = '\0';
        }
    }

    int best=0; const TVRemoteModel* best_m=nullptr;
    for (int i = 0; i < model_registry_count(); i++) {
        const TVRemoteModel* m = model_registry_get(i);
        int score = tv_detect_score(m, &res);
        if (score > best) { best=score; best_m=m; }
    }
    res.found = (best_m != nullptr);
    res.model  = best_m;

    OSLockMutex(&s_mutex);
    s_result = res;
    s_done   = true;
    OSUnlockMutex(&s_mutex);
    s_running = false;
    return 0;
}

void tv_detect_start()
{
    if (!s_mutex_init) { OSInitMutex(&s_mutex); s_mutex_init=true; }
    if (s_running) return;
    s_done=false; s_running=true;
    memset(&s_result,0,sizeof(s_result));
    OSCreateThread(&s_thread, detect_thread, 0, nullptr,
                   s_stack+sizeof(s_stack), sizeof(s_stack),
                   16, OS_THREAD_ATTRIB_AFFINITY_ANY);
    OSSetThreadName(&s_thread, "tv_detect");
    OSResumeThread(&s_thread);
}

bool tv_detect_done()
{
    if (!s_mutex_init) return false;
    OSLockMutex(&s_mutex);
    bool d = s_done;
    OSUnlockMutex(&s_mutex);
    return d;
}

const TVDetectResult* tv_detect_result() { return &s_result; }

int tv_detect_score(const TVRemoteModel* m, const TVDetectResult* r)
{
    if (!m || !r) return 0;
    int score = 0;
    if (m->cec_vendor && m->cec_vendor == r->cec_vendor) score += 100;
    if (m->cec_osd_name_prefix && m->cec_osd_name_prefix[0] && r->osd_name[0]) {
        size_t plen = strlen(m->cec_osd_name_prefix);
        if (strncasecmp(r->osd_name, m->cec_osd_name_prefix, plen) == 0) score += 50;
    }
    return score;
}
