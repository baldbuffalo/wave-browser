// zlib_stubs.c
// libcurl was compiled with zlib/gzip support but libz is not available
// in this devkitPro image. These stubs satisfy the linker and disable
// gzip/deflate decompression — curl still works for plain responses.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Minimal z_stream struct to satisfy the linker
typedef struct {
    const unsigned char *next_in;
    unsigned int avail_in;
    unsigned long total_in;
    unsigned char *next_out;
    unsigned int avail_out;
    unsigned long total_out;
    const char *msg;
    void *state;
    void *zalloc;
    void *zfree;
    void *opaque;
    int data_type;
    unsigned long adler;
    unsigned long reserved;
} z_stream;

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)

const char* zlibVersion(void) { return "1.0.0-stub"; }

int inflateInit_(z_stream *s, const char *v, int n) {
    (void)s; (void)v; (void)n;
    return Z_STREAM_ERROR;
}

int inflateInit2_(z_stream *s, int bits, const char *v, int n) {
    (void)s; (void)bits; (void)v; (void)n;
    return Z_STREAM_ERROR;
}

int inflate(z_stream *s, int flush) {
    (void)s; (void)flush;
    return Z_STREAM_ERROR;
}

int inflateEnd(z_stream *s) {
    (void)s;
    return Z_OK;
}
