// Wii U hosted HTTP frame server. The receiver uses normal JPEG responses
// and browser fetch/blob APIs rather than multipart MJPEG for broad compatibility.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "mirror_state.h"
#include "platform.h"
namespace mirror_screen {
namespace {
constexpr int kBacklog=2;
constexpr size_t kRequestSize=1024;
constexpr const char* kStatusPath="fs:/vol/external01/wiiu/apps/WaveBrowser/mirror-screen-status.cfg";
std::atomic<bool> gRunning{false},gActive{false};
std::atomic<int> gListener{-1},gClientSocket{-1};
std::atomic<uint32_t> gLastClientRequestMs{0};
MirrorThread gServerThread;
OSMutex gFrameMutex;
uint8_t gLatestFrame[kJpegCapacity];
size_t gLatestSize=0;
uint32_t gSequence=0;

const char kPage[]=
"<!doctype html><html lang='en'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'>"
"<meta name='theme-color' content='#101827'><title>Mirror Screen</title>"
"<style>*{box-sizing:border-box}body{margin:0;background:#101827;color:#eef3fa;font:16px system-ui,sans-serif;min-height:100vh;display:flex;flex-direction:column}"
"header{padding:18px 22px;font-weight:700}main{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:12px;gap:16px}"
".screen{width:100%;max-width:1100px;background:#05080d;border-radius:14px;overflow:hidden;min-height:180px;display:flex;align-items:center;justify-content:center}"
"img{display:block;width:100%;height:auto}#status{color:#aab8ca;text-align:center}footer{padding:16px;text-align:center;font-size:13px;color:#8d9bad}</style>"
"</head><body><header>Mirror Screen</header><main><div class='screen'><img id='screen' alt='Wii U TV output'></div>"
"<div id='status'>Connecting to Wii U…</div></main><footer>Direct local Wi-Fi stream · TV video only</footer><script>"
"function deviceName(){var ua=navigator.userAgent||'',p=navigator.platform||'';"
"if(/iPad/i.test(ua)||(p==='MacIntel'&&navigator.maxTouchPoints>1))return 'iPad';"
"if(/iPhone|iPod/i.test(ua))return 'iPhone';if(/Android/i.test(ua))return /Mobile/i.test(ua)?'Android phone':'Android tablet';"
"if(/Windows/i.test(ua))return 'Windows PC';if(/Macintosh|Mac OS X/i.test(ua))return 'Mac';"
"if(/Linux/i.test(ua))return 'Linux PC';return 'Browser device'}"
"var status=document.getElementById('status'),screen=document.getElementById('screen');"
"var sequence=0,previousUrl=null,running=true;"
"async function getFrame(){if(!running)return;try{"
"var response=await fetch('/frame.jpg?after='+sequence,{cache:'no-store'});"
"if(response.status===200){var next=parseInt(response.headers.get('X-Frame-Sequence')||'0',10);"
"var blob=await response.blob(),url=URL.createObjectURL(blob),old=previousUrl;"
"screen.onload=function(){if(old)URL.revokeObjectURL(old);};screen.src=url;previousUrl=url;"
"if(next)sequence=next;status.textContent='Receiving TV output';"
"}else if(response.status!==204){status.textContent='Waiting for video frames…';}"
"}catch(e){status.textContent='Connection interrupted. Reconnecting…';}"
"if(running)setTimeout(getFrame,0);}"
"fetch('/client?device='+encodeURIComponent(deviceName()),{cache:'no-store'})"
".then(function(r){if(!r.ok)throw new Error('Wii U did not accept the connection');getFrame();})"
".catch(function(e){status.textContent=e.message;});"
"window.addEventListener('pagehide',function(){running=false;});"
"</script></body></html>";

void writeStatus(bool loaded,bool active,const char* device){
 FILE* f=std::fopen(kStatusPath,"w");if(!f)return;
 std::fprintf(f,"loaded=%d\nactive=%d\nsource=TV\ndevice=%s\n",loaded?1:0,active?1:0,device?device:"");
 std::fclose(f);
}
bool sendAll(int sock,const void* data,size_t size){
 const uint8_t* p=static_cast<const uint8_t*>(data);size_t sent=0;
 while(sent<size&&gRunning.load()){int n=send(sock,p+sent,size-sent,0);if(n<=0)return false;sent+=static_cast<size_t>(n);}
 return sent==size;
}
bool sendResponse(int sock,int status,const char* reason,const char* type,const char* body,size_t size){
 char header[320];int n=std::snprintf(header,sizeof(header),
 "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
 status,reason,type,static_cast<unsigned>(size));
 return n>0&&sendAll(sock,header,static_cast<size_t>(n))&&(!size||sendAll(sock,body,size));
}
void decodeValue(const char* in,char* out,size_t cap){
 size_t j=0;for(size_t i=0;in&&in[i]&&j+1<cap;i++){
  unsigned char c=static_cast<unsigned char>(in[i]);
  if(c=='%'&&in[i+1]&&in[i+2]){char hex[3]={in[i+1],in[i+2],0};c=static_cast<unsigned char>(std::strtoul(hex,nullptr,16));i+=2;}
  else if(c=='+')c=' ';
  if(c<32||c==127||c=='&')continue;out[j++]=static_cast<char>(c);
 }out[j]='\0';
}
bool readRequest(int sock,char* req,size_t cap){
 size_t used=0;while(used+1<cap){int n=recv(sock,req+used,cap-used-1,0);if(n<=0)return false;
  used+=static_cast<size_t>(n);req[used]='\0';if(std::strstr(req,"\r\n\r\n"))return true;}return false;
}
uint32_t getAfterSequence(const char* path){
 const char* q=std::strstr(path,"after=");if(!q)return 0;
 return static_cast<uint32_t>(std::strtoul(q+6,nullptr,10));
}
void sendFrame(int sock,const char* path){
 uint32_t after=getAfterSequence(path),sequence=0;size_t size=0;
 uint8_t* local=static_cast<uint8_t*>(std::malloc(kJpegCapacity));if(!local)return;
 uint32_t waited=0;
 while(gRunning.load()&&gActive.load()&&waited<1200){
  OSLockMutex(&gFrameMutex);sequence=gSequence;
  if(sequence!=after&&gLatestSize>0&&gLatestSize<=kJpegCapacity){size=gLatestSize;std::memcpy(local,gLatestFrame,size);}
  OSUnlockMutex(&gFrameMutex);
  if(size)break;
  mirrorSleepMs(4);waited+=4;
 }
 if(!size){
  const char* empty="";
  sendResponse(sock,204,"No Content","text/plain",empty,0);
  std::free(local);return;
 }
 char header[320];int n=std::snprintf(header,sizeof(header),
 "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\nX-Frame-Sequence: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
 static_cast<unsigned>(size),sequence);
 if(n>0&&sendAll(sock,header,static_cast<size_t>(n)))sendAll(sock,local,size);
 std::free(local);
}
void handleRequest(int sock){
 char req[kRequestSize]{};if(!readRequest(sock,req,sizeof(req)))return;
 char method[8]{},path[384]{};
 if(std::sscanf(req,"%7s %383s",method,path)!=2||std::strcmp(method,"GET")!=0){
  const char* body="Only GET is supported";sendResponse(sock,400,"Bad Request","text/plain",body,std::strlen(body));return;
 }
 if(std::strcmp(path,"/")==0||std::strcmp(path,"/index.html")==0){
  sendResponse(sock,200,"OK","text/html; charset=utf-8",kPage,sizeof(kPage)-1);return;
 }
 if(std::strncmp(path,"/client?",8)==0){
  const char* q=std::strstr(path+8,"device=");char device[48]="Browser device";if(q)decodeValue(q+7,device,sizeof(device));
  gActive.store(true);gLastClientRequestMs.store(mirrorNowMs());writeStatus(true,true,device);
  const char* body="Mirror Screen connected";sendResponse(sock,200,"OK","text/plain; charset=utf-8",body,std::strlen(body));return;
 }
 if(std::strncmp(path,"/frame.jpg",10)==0){
  if(!gActive.load()){const char* body="Open the Mirror Screen page first";sendResponse(sock,409,"Conflict","text/plain",body,std::strlen(body));return;}
  gLastClientRequestMs.store(mirrorNowMs());sendFrame(sock,path);return;
 }
 const char* body="Not found";sendResponse(sock,404,"Not Found","text/plain",body,std::strlen(body));
}
int serverEntry(int,const char**){
 while(gRunning.load()){
  if(gActive.load()&&static_cast<uint32_t>(mirrorNowMs()-gLastClientRequestMs.load())>3500u){
   gActive.store(false);writeStatus(true,false,"");
  }
  int listener=gListener.load();if(listener<0)break;
  sockaddr_in peer{};socklen_t peerLen=sizeof(peer);
  int client=accept(listener,reinterpret_cast<sockaddr*>(&peer),&peerLen);
  if(client<0){mirrorSleepMs(5);continue;}
  int blocking=0;setsockopt(client,SOL_SOCKET,SO_NONBLOCK,&blocking,sizeof(blocking));
  gClientSocket.store(client);handleRequest(client);shutdown(client,2);close(client);gClientSocket.store(-1);
 }
 return 0;
}
}
bool clientActive(){return gActive.load();}
void publishFrame(const uint8_t* jpeg,size_t len){
 if(!jpeg||!len||len>kJpegCapacity)return;
 OSLockMutex(&gFrameMutex);std::memcpy(gLatestFrame,jpeg,len);gLatestSize=len;++gSequence;if(gSequence==0)++gSequence;OSUnlockMutex(&gFrameMutex);
}
bool httpStart(){
 if(gRunning.load())return true;
 OSInitMutex(&gFrameMutex);gLatestSize=0;gSequence=0;gActive.store(false);
 int sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(sock<0)return false;
 int yes=1;setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
 int nonblock=1;setsockopt(sock,SOL_SOCKET,SO_NONBLOCK,&nonblock,sizeof(nonblock));
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=htons(kHttpPort);
 if(bind(sock,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0||listen(sock,kBacklog)<0){close(sock);return false;}
 gListener.store(sock);gRunning.store(true);
 if(!gServerThread.start(serverEntry,96*1024,22,OS_THREAD_ATTRIB_AFFINITY_CPU2,"MirrorScreen HTTP")){
  gRunning.store(false);close(sock);gListener.store(-1);return false;
 }
 writeStatus(true,false,"");MIRROR_LOG("receiver listening on TCP port %u",kHttpPort);return true;
}
void httpStop(){
 if(!gRunning.exchange(false))return;gActive.store(false);
 int client=gClientSocket.exchange(-1);if(client>=0)shutdown(client,2);
 int listener=gListener.exchange(-1);if(listener>=0){shutdown(listener,2);close(listener);}
 gServerThread.join();writeStatus(false,false,"");
}
}
