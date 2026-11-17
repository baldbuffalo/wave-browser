#pragma once
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <coreinit/thread.h>
#include <curl/curl.h>
#include "unzip.h"

#define MIRROR_AROMA_PLUGIN_DIR "fs:/vol/external01/wiiu/environments/aroma/plugins"
#define MIRROR_PLUGIN_PATH "fs:/vol/external01/wiiu/environments/aroma/plugins/MirrorScreen.wps"
#define MIRROR_PLUGIN_TEMP_PATH "fs:/vol/external01/wiiu/environments/aroma/plugins/MirrorScreen.wps.part"
#define MIRROR_ZIP_TEMP_PATH "fs:/vol/external01/wiiu/apps/WaveBrowser/MirrorScreen-download.zip"
#define MIRROR_STATUS_PATH "fs:/vol/external01/wiiu/apps/WaveBrowser/mirror-screen-status.cfg"
#define MIRROR_PLUGIN_DOWNLOAD_URL "https://nightly.link/baldbuffalo/wave-browser/workflows/plugin-build.yml/main/mirror-plugin-wps.zip"

namespace mirror_installer {
enum State { IDLE=0, DOWNLOADING=1, EXTRACTING=2, INSTALLED=3, FAILED=-1 };
static std::atomic<int> s_state{IDLE};
static std::atomic<int> s_progress{0};
static std::atomic<bool> s_done{false};
static bool s_started=false;
static OSThread s_thread{};
static uint8_t* s_stack=nullptr;
static constexpr uint32_t kStackSize=128*1024;

inline bool isInstalled() {
    struct stat st{};
    return stat(MIRROR_PLUGIN_PATH,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0;
}
inline int state(){return s_state.load();}
inline int progress(){return s_progress.load();}
inline bool isRunning(){return s_started && !s_done.load();}

static int progressCallback(void*,curl_off_t total,curl_off_t now,curl_off_t,curl_off_t) {
    if(total>0) {
        int pct=static_cast<int>((static_cast<double>(now)*100.0)/static_cast<double>(total));
        if(pct<0)pct=0; if(pct>100)pct=100; s_progress.store(pct);
    }
    return 0;
}
static size_t fileWrite(void* ptr,size_t size,size_t count,void* user) {
    return fwrite(ptr,size,count,static_cast<FILE*>(user));
}
static bool extractPlugin() {
    unzFile zip=unzOpen(MIRROR_ZIP_TEMP_PATH);
    if(!zip)return false;
    bool extracted=false;
    if(unzGoToFirstFile(zip)==UNZ_OK) do {
        char name[256]{}; unz_file_info info{};
        if(unzGetCurrentFileInfo(zip,&info,name,sizeof(name)-1,nullptr,0,nullptr,0)!=UNZ_OK)break;
        const char* base=strrchr(name,'/'); base=base?base+1:name;
        if(strcmp(base,"MirrorScreen.wps")!=0)continue;
        if(unzOpenCurrentFile(zip)!=UNZ_OK)break;
        FILE* out=fopen(MIRROR_PLUGIN_TEMP_PATH,"wb");
        if(!out){unzCloseCurrentFile(zip);break;}
        bool ok=true; uint8_t buffer[8192]; int n=0;
        while((n=unzReadCurrentFile(zip,buffer,sizeof(buffer)))>0) {
            if(fwrite(buffer,1,static_cast<size_t>(n),out)!=static_cast<size_t>(n)){ok=false;break;}
        }
        if(n<0)ok=false;
        if(fclose(out)!=0)ok=false;
        unzCloseCurrentFile(zip);
        struct stat st{};
        extracted=ok && stat(MIRROR_PLUGIN_TEMP_PATH,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0;
        break;
    } while(unzGoToNextFile(zip)==UNZ_OK);
    unzClose(zip);
    return extracted;
}
static bool performInstall() {
    struct stat dir{};
    if(stat(MIRROR_AROMA_PLUGIN_DIR,&dir)!=0 || !S_ISDIR(dir.st_mode))return false;
    remove(MIRROR_ZIP_TEMP_PATH); remove(MIRROR_PLUGIN_TEMP_PATH);
    FILE* file=fopen(MIRROR_ZIP_TEMP_PATH,"wb"); if(!file)return false;
    CURL* curl=curl_easy_init();
    if(!curl){fclose(file);remove(MIRROR_ZIP_TEMP_PATH);return false;}
    curl_easy_setopt(curl,CURLOPT_URL,MIRROR_PLUGIN_DOWNLOAD_URL);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,fileWrite);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,file);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl,CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"WaveBrowser-MirrorScreen/1.0");
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,20L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,180L);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progressCallback);
    CURLcode result=curl_easy_perform(curl);
    long code=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
    curl_easy_cleanup(curl);
    bool closeOk=fclose(file)==0;
    if(result!=CURLE_OK || code<200 || code>=300 || !closeOk){remove(MIRROR_ZIP_TEMP_PATH);return false;}

    s_state.store(EXTRACTING);
    if(!extractPlugin()){remove(MIRROR_PLUGIN_TEMP_PATH);remove(MIRROR_ZIP_TEMP_PATH);return false;}
    remove(MIRROR_ZIP_TEMP_PATH);
    const char* backup="fs:/vol/external01/wiiu/environments/aroma/plugins/MirrorScreen.wps.bak";
    remove(backup);
    bool hadOld=rename(MIRROR_PLUGIN_PATH,backup)==0;
    if(rename(MIRROR_PLUGIN_TEMP_PATH,MIRROR_PLUGIN_PATH)!=0){
        if(hadOld)rename(backup,MIRROR_PLUGIN_PATH);
        remove(MIRROR_PLUGIN_TEMP_PATH);return false;
    }
    remove(backup); remove(MIRROR_STATUS_PATH);
    return true;
}
static int workerEntry(int,void*) {
    s_state.store(DOWNLOADING); s_progress.store(0);
    bool ok=performInstall();
    s_state.store(ok?INSTALLED:FAILED); s_done.store(true);
    return 0;
}
inline bool start() {
    if(s_started)return false;
    s_state.store(DOWNLOADING); s_progress.store(0); s_done.store(false);
    s_stack=static_cast<uint8_t*>(malloc(kStackSize));
    if(!s_stack){s_state.store(FAILED);s_done.store(true);return false;}
    if(!OSCreateThread(&s_thread,workerEntry,0,nullptr,s_stack+kStackSize,kStackSize,24,OS_THREAD_ATTRIB_AFFINITY_ANY)){
        free(s_stack);s_stack=nullptr;s_state.store(FAILED);s_done.store(true);return false;
    }
    s_started=true; OSSetThreadName(&s_thread,"MirrorScreen installer"); OSResumeThread(&s_thread);
    return true;
}
inline void poll() {
    if(!s_started || !s_done.load())return;
    OSJoinThread(&s_thread,nullptr); free(s_stack); s_stack=nullptr; s_started=false;
}
}
inline bool mirror_plugin_is_installed(){return mirror_installer::isInstalled();}
inline bool mirror_installer_start(){return mirror_installer::start();}
inline bool mirror_installer_is_running(){return mirror_installer::isRunning();}
inline int mirror_installer_progress(){return mirror_installer::progress();}
inline int mirror_installer_state(){return mirror_installer::state();}
inline void mirror_installer_poll(){mirror_installer::poll();}
