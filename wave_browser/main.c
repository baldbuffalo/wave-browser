#include <coreinit/network.h>
#include <coreinit/socket.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/cache.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int fetch_latest_release_simple(char *out_tag, size_t tag_size) {
    int ret = 1;

    if (netInitialize() != 0) return 1;

    struct hostent *he = gethostbyname("api.github.com");
    if (!he) return 1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 1;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return 1;
    }

    const char *req = 
        "GET /repos/baldbuffalo/wave-browser/releases/latest HTTP/1.0\r\n"
        "Host: api.github.com\r\n"
        "User-Agent: wave-browser\r\n"
        "\r\n";

    send(sock, req, strlen(req), 0);

    char buffer[4096];
    int bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (bytes > 0) {
        buffer[bytes] = 0;
        char *tag_ptr = strstr(buffer, "\"tag_name\":\"");
        if (tag_ptr) {
            tag_ptr += 12;
            char *end = strchr(tag_ptr, '"');
            if (end) {
                size_t len = end - tag_ptr;
                if (len >= tag_size) len = tag_size - 1;
                strncpy(out_tag, tag_ptr, len);
                out_tag[len] = 0;
                ret = 0; // success
            }
        }
    }

    closesocket(sock);
    return ret;
}
