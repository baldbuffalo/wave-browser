#include "gx2_gl_driver.h"

#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/state.h>
#include <gx2/surface.h>

#include <cstring>

namespace {

bool makeCurrentChecked(GX2GLContext* context)
{
    if (!context || !context->initialized)
        return false;

    GX2SetContextState(&context->gx2State);
    if (context->colorBuffer)
        GX2SetColorBuffer(context->colorBuffer);
    if (context->depthBuffer)
        GX2SetDepthBuffer(context->depthBuffer);
    if (context->fetchShader)
        GX2SetFetchShader(context->fetchShader);
    if (context->vertexShader)
        GX2SetVertexShader(context->vertexShader);
    if (context->pixelShader)
        GX2SetPixelShader(context->pixelShader);
    return true;
}

bool mapPrimitive(uint32_t glMode, GX2PrimitiveMode* outMode)
{
    if (!outMode)
        return false;

    switch (glMode) {
    case 0x0000: *outMode = GX2_PRIMITIVE_MODE_POINTS; return true;         // GL_POINTS
    case 0x0001: *outMode = GX2_PRIMITIVE_MODE_LINES; return true;          // GL_LINES
    case 0x0002: *outMode = GX2_PRIMITIVE_MODE_LINE_LOOP; return true;      // GL_LINE_LOOP
    case 0x0003: *outMode = GX2_PRIMITIVE_MODE_LINE_STRIP; return true;     // GL_LINE_STRIP
    case 0x0004: *outMode = GX2_PRIMITIVE_MODE_TRIANGLES; return true;      // GL_TRIANGLES
    case 0x0005: *outMode = GX2_PRIMITIVE_MODE_TRIANGLE_STRIP; return true; // GL_TRIANGLE_STRIP
    case 0x0006: *outMode = GX2_PRIMITIVE_MODE_TRIANGLE_FAN; return true;   // GL_TRIANGLE_FAN
    default: return false;
    }
}

} // namespace

extern "C" bool
gx2glInitializeContext(GX2GLContext* context)
{
    if (!context)
        return false;

    std::memset(context, 0, sizeof(*context));
    GX2SetupContextStateEx(&context->gx2State, TRUE);
    context->initialized = true;
    GX2SetContextState(&context->gx2State);
    return true;
}

extern "C" void
gx2glDestroyContext(GX2GLContext* context)
{
    if (!context)
        return;

    // GX2 resources are owned by the caller; this context only stores borrowed
    // pointers to render targets and compiled GX2 shader objects.
    std::memset(context, 0, sizeof(*context));
}

extern "C" bool
gx2glMakeCurrent(GX2GLContext* context)
{
    return makeCurrentChecked(context);
}

extern "C" void
gx2glSetRenderTargets(GX2GLContext* context,
                      GX2ColorBuffer* colorBuffer,
                      GX2DepthBuffer* depthBuffer)
{
    if (!context || !context->initialized)
        return;

    context->colorBuffer = colorBuffer;
    context->depthBuffer = depthBuffer;
    makeCurrentChecked(context);
}

extern "C" void
gx2glBindPipeline(GX2GLContext* context,
                  const GX2FetchShader* fetchShader,
                  const GX2VertexShader* vertexShader,
                  const GX2PixelShader* pixelShader)
{
    if (!context || !context->initialized)
        return;

    context->fetchShader = fetchShader;
    context->vertexShader = vertexShader;
    context->pixelShader = pixelShader;
    makeCurrentChecked(context);
}

extern "C" bool
gx2glSetVertexAttribute(GX2GLContext* context,
                        uint32_t index,
                        uint32_t byteSize,
                        uint32_t stride,
                        const void* data)
{
    if (!makeCurrentChecked(context) || !data || !byteSize || !stride)
        return false;

    GX2SetAttribBuffer(index, byteSize, stride, data);
    return true;
}

extern "C" void
gx2glClearColor(GX2GLContext* context,
                float red, float green, float blue, float alpha)
{
    if (!makeCurrentChecked(context) || !context->colorBuffer)
        return;

    GX2ClearColor(context->colorBuffer, red, green, blue, alpha);
}

extern "C" void
gx2glClearDepthStencil(GX2GLContext* context,
                       float depth, uint8_t stencil,
                       GX2ClearFlags flags)
{
    if (!makeCurrentChecked(context) || !context->depthBuffer)
        return;

    GX2ClearDepthStencilEx(context->depthBuffer, depth, stencil, flags);
}

extern "C" bool
gx2glDrawArrays(GX2GLContext* context,
                uint32_t primitiveMode,
                uint32_t first,
                uint32_t count,
                uint32_t instances)
{
    GX2PrimitiveMode mode;
    if (!makeCurrentChecked(context)
        || !context->fetchShader
        || !context->vertexShader
        || !context->pixelShader
        || !count
        || !instances
        || !mapPrimitive(primitiveMode, &mode))
        return false;

    GX2DrawEx(mode, count, first, instances);
    return true;
}

extern "C" bool
gx2glDrawIndexed(GX2GLContext* context,
                 uint32_t primitiveMode,
                 uint32_t count,
                 GX2IndexType indexType,
                 const void* indices,
                 uint32_t byteOffset,
                 uint32_t instances)
{
    GX2PrimitiveMode mode;
    if (!makeCurrentChecked(context)
        || !context->fetchShader
        || !context->vertexShader
        || !context->pixelShader
        || !indices
        || !count
        || !instances
        || !mapPrimitive(primitiveMode, &mode))
        return false;

    if (indexType != GX2_INDEX_TYPE_U16 && indexType != GX2_INDEX_TYPE_U32
        && indexType != GX2_INDEX_TYPE_U16_LE && indexType != GX2_INDEX_TYPE_U32_LE)
        return false;

    GX2DrawIndexedEx(mode, count, indexType, indices, byteOffset, instances);
    return true;
}

extern "C" void
gx2glFlush(void)
{
    GX2Flush();
}
