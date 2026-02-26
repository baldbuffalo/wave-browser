#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>
#include <socketcore/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CURRENT_VERSION "v0.1.0"
#define GITHUB_HOST "api.github.com"
#define GITHUB_PORT 443
#define GITHUB_API "/repos/baldbuffalo/wave-browser/releases/latest"

struct MemoryStruct {
    char *memory;
    size_t size;
};

int fetch_latest_release(char *out_tag, size_t tag_size)
{
    SC_Socket sock;
    SC_SockAddr addr;
    char buffer[4096];
    int ret;

    // Resolve host
    ret = SC_GetAddr(GITHUB_HOST, GITHUB_PORT, &addr);
    if(ret != 0) return 1;

    // Open socket
    sock = SC_Socket(SC_AF_INET, SC_SOCK_STREAM, 0);
    if(sock < 0) return 1;

    // Connect
    ret = SC_Connect(sock, &addr, sizeof(addr));
    if(ret != 0) {
        SC_Close(sock);
        return 1;
    }

    // Send HTTPS request (Wii U does not support SSL by default; this is plain HTTP request simulation)
    char request[512];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: wave-browser\r\n\r\n",
             GITHUB_API, GITHUB_HOST);

    SC_Send(sock, request, strlen(request), 0);

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    // Receive response
    while((ret = SC_Recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[ret] = 0;
        char *ptr = realloc(chunk.memory, chunk.size + ret + 1);
        if(!ptr) {
            free(chunk.memory);
            SC_Close(sock);
            return 1;
        }
        chunk.memory = ptr;
        memcpy(&(chunk.memory[chunk.size]), buffer, ret);
        chunk.size += ret;
        chunk.memory[chunk.size] = 0;
    }

    SC_Close(sock);

    // Parse JSON manually for "tag_name"
    char *tag_ptr = strstr(chunk.memory, "\"tag_name\":\"");
    if(tag_ptr) {
        tag_ptr += 12;
        char *end = strchr(tag_ptr,'"');
        if(end) {
            size_t len = end - tag_ptr;
            if(len >= tag_size) len = tag_size - 1;
            strncpy(out_tag, tag_ptr, len);
            out_tag[len] = 0;
        }
    }

    free(chunk.memory);
    return 0;
}

int main(void)
{
    ProcUIInit(NULL);
    VPADInit();

    printf("Wave Browser\n");
    printf("Checking for updates...\n");

    char latest_tag[64] = {0};

    if(fetch_latest_release(latest_tag, sizeof(latest_tag)) == 0) {
        if(strcmp(latest_tag, CURRENT_VERSION) != 0) {
            printf("Update available: %s\n", latest_tag);
        } else {
            printf("You are up to date.\n");
        }
    } else {
        printf("Update check failed.\n");
    }

    while (ProcUIIsRunning())
    {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        (void)vpad;
        (void)error;
        usleep(50000);
    }

    ProcUIShutdown();
    return 0;
}
