#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define HOST "api.github.com"
#define PATH "/repos/baldbuffalo/wave-browser/releases/latest"

__asm__(".global __rpx_start\n\t"
        "__rpx_start: b main");

// -------------------- Fetch latest release safely --------------------
int fetch_latest_release(char *out_tag, size_t tag_size) {
    if (!out_tag || tag_size == 0) return 1;

    struct hostent *he = gethostbyname(HOST);
    if (!he) return 1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 1;
    }

    const char *req =
        "GET " PATH " HTTP/1.1\r\n"
        "Host: " HOST "\r\n"
        "User-Agent: wave-browser\r\n"
        "Connection: close\r\n"
        "\r\n";

    if (send(sock, req, strlen(req), 0) < 0) {
        close(sock);
        return 1;
    }

    size_t buffer_size = 16384; // 16 KB
    size_t offset = 0;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        close(sock);
        return 1;
    }

    ssize_t bytes;
    while ((bytes = recv(sock, buffer + offset, buffer_size - offset - 1, 0)) > 0) {
        offset += bytes;
        if (offset >= buffer_size - 1) {
            buffer_size *= 2;
            char *new_buf = realloc(buffer, buffer_size);
            if (!new_buf) {
                free(buffer);
                close(sock);
                return 1;
            }
            buffer = new_buf;
        }
    }
    buffer[offset] = '\0';
    close(sock);

    int ret = 1;
    char *tag_ptr = strstr(buffer, "\"tag_name\":\"");
    if (tag_ptr) {
        tag_ptr += strlen("\"tag_name\":\"");
        char *end = strchr(tag_ptr, '"');
        if (end) {
            size_t len = end - tag_ptr;
            if (len >= tag_size) len = tag_size - 1;
            strncpy(out_tag, tag_ptr, len);
            out_tag[len] = '\0';
            ret = 0;
        }
    }

    free(buffer);
    return ret;
}

// -------------------- MAIN --------------------
int main(void) {
    ProcUIInit(NULL);
    VPADInit();

    char latest[64] = {0};
    fetch_latest_release(latest, sizeof(latest));
    // latest[] now contains the version tag if fetch was successful

    // Main loop
    while (ProcUIIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        ProcUIProcessMessages(TRUE);
        usleep(16000); // ~60 FPS
    }

    ProcUIShutdown();
    return 0;
}
