#include <coreinit/core.h>
#include <coreinit/debug.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>

#include <nsysnet/socket.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#define HOST "api.github.com"
#define PATH "/repos/baldbuffalo/wave-browser/releases/latest"

// -------------------- Fetch latest release --------------------
int fetch_latest_release(char *out_tag, size_t tag_size) {
    struct hostent *he = gethostbyname(HOST);
    if (!he) {
        OSReport("DNS failed\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        OSReport("Socket creation failed\n");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        OSReport("Connect failed\n");
        close(sock);
        return 1;
    }

    const char *req =
        "GET " PATH " HTTP/1.0\r\n"
        "Host: " HOST "\r\n"
        "User-Agent: wave-browser\r\n"
        "\r\n";

    send(sock, req, strlen(req), 0);

    char buffer[8192];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    int ret = 1;

    if (bytes > 0) {
        buffer[bytes] = 0;

        char *tag_ptr = strstr(buffer, "\"tag_name\":\"");
        if (tag_ptr) {
            tag_ptr += 12;
            char *end = strchr(tag_ptr, '"');
            if (end) {
                size_t len = end - tag_ptr;
                if (len >= tag_size)
                    len = tag_size - 1;

                strncpy(out_tag, tag_ptr, len);
                out_tag[len] = 0;
                ret = 0;
            }
        }
    }

    close(sock);
    return ret;
}

// -------------------- MAIN --------------------
int main(void) {

    ProcUIInit(NULL);
    VPADInit();

    // REQUIRED for networking
    socket_lib_init();

    OSReport("Wave Browser starting...\n");

    char latest[64] = {0};

    if (fetch_latest_release(latest, sizeof(latest)) == 0)
        OSReport("Latest release: %s\n", latest);
    else
        OSReport("Failed to fetch release\n");

    // Basic loop
    while (ProcUIIsRunning()) {

        VPADStatus vpad;
        VPADReadError error;

        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        ProcUIProcessMessages(TRUE);

        usleep(16000);
    }

    socket_lib_finish();
    ProcUIShutdown();
    return 0;
}
