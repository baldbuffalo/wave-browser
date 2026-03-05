// brotli_stubs.c
// libcurl in this devkitPro image was compiled with brotli support,
// but libbrotli is not available. These stubs satisfy the linker and
// effectively disable brotli decompression (curl will still work fine
// for gzip and plain responses).

#include <stddef.h>
#include <stdint.h>

typedef void* BrotliDecoderState;
typedef enum { BROTLI_DECODER_RESULT_ERROR = 0 } BrotliDecoderResult;

BrotliDecoderState* BrotliDecoderCreateInstance(void *a, void *b, void *c) { return NULL; }
void  BrotliDecoderDestroyInstance(BrotliDecoderState *s) { (void)s; }
BrotliDecoderResult BrotliDecoderDecompressStream(
    BrotliDecoderState *s, size_t *a, const uint8_t **b,
    size_t *c, uint8_t **d, size_t *e) { (void)s;(void)a;(void)b;(void)c;(void)d;(void)e; return BROTLI_DECODER_RESULT_ERROR; }
int   BrotliDecoderGetErrorCode(BrotliDecoderState *s) { (void)s; return 0; }
