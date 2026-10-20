#pragma once

#include <cstdint>
#include <gx2/context.h>
#include <gx2/enum.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GX2GL is the native GX2 execution layer for the Wii U OpenGL compatibility
 * driver. It deliberately does not call OpenGL. Higher layers validate API
 * state and translate supported operations into these GX2 operations.
 *
 * This is an incremental backend: it is not yet a claim of OpenGL 4.6
 * conformance. Unsupported operations must be rejected by the API layer until
 * they are implemented or emulated.
 */
typedef struct GX2GLContext {
    alignas(0x100) GX2ContextState gx2State;
    GX2ColorBuffer* colorBuffer;
    GX2DepthBuffer* depthBuffer;
    const GX2FetchShader* fetchShader;
    const GX2VertexShader* vertexShader;
    const GX2PixelShader* pixelShader;
    bool initialized;
} GX2GLContext;

bool gx2glInitializeContext(GX2GLContext* context);
void gx2glDestroyContext(GX2GLContext* context);
bool gx2glMakeCurrent(GX2GLContext* context);

void gx2glSetRenderTargets(GX2GLContext* context,
                           GX2ColorBuffer* colorBuffer,
                           GX2DepthBuffer* depthBuffer);
void gx2glBindPipeline(GX2GLContext* context,
                       const GX2FetchShader* fetchShader,
                       const GX2VertexShader* vertexShader,
                       const GX2PixelShader* pixelShader);

/* Attribute data must already reside in memory accessible to GX2. */
bool gx2glSetVertexAttribute(GX2GLContext* context,
                             uint32_t index,
                             uint32_t byteSize,
                             uint32_t stride,
                             const void* data);

void gx2glClearColor(GX2GLContext* context,
                     float red, float green, float blue, float alpha);
void gx2glClearDepthStencil(GX2GLContext* context,
                            float depth, uint8_t stencil,
                            GX2ClearFlags flags);
bool gx2glDrawArrays(GX2GLContext* context,
                     uint32_t primitiveMode,
                     uint32_t first,
                     uint32_t count,
                     uint32_t instances);
bool gx2glDrawIndexed(GX2GLContext* context,
                      uint32_t primitiveMode,
                      uint32_t count,
                      GX2IndexType indexType,
                      const void* indices,
                      uint32_t byteOffset,
                      uint32_t instances);
void gx2glFlush(void);

#ifdef __cplusplus
}
#endif
