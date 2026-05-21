/*
 * wkcbase.h — WKC base definitions stub for Wii U / Aroma port
 */
#ifndef _WKC_BASE_H_
#define _WKC_BASE_H_
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
#  define WKC_BEGIN_C_LINKAGE extern "C" {
#  define WKC_END_C_LINKAGE   }
#else
#  define WKC_BEGIN_C_LINKAGE
#  define WKC_END_C_LINKAGE
#endif
#define WKC_PEER_API
#define WKC_API
#define WKC_EXPORT
typedef uint64_t wkc_uint64;
typedef int64_t  wkc_int64;
typedef unsigned int  WKCColor;
typedef float         WKCFloat;
typedef struct WKCPoint_      { int fX, fY; }             WKCPoint;
typedef struct WKCSize_       { int fWidth, fHeight; }    WKCSize;
typedef struct WKCRect_       { WKCPoint fPoint; WKCSize fSize; } WKCRect;
typedef struct WKCFloatPoint_ { float fX, fY; }           WKCFloatPoint;
typedef struct WKCFloatSize_  { float fWidth, fHeight; }  WKCFloatSize;
typedef struct WKCFloatRect_  { WKCFloatPoint fPoint; WKCFloatSize fSize; } WKCFloatRect;

/* Integer geometry */
#define WKCPoint_Set(p,x,y)        do { (p)->fX=(x); (p)->fY=(y); } while(0)
#define WKCSize_Set(s,w,h)         do { (s)->fWidth=(w); (s)->fHeight=(h); } while(0)
#define WKCRect_Set(r,x,y,w,h)    do { WKCPoint_Set(&(r)->fPoint,(x),(y)); WKCSize_Set(&(r)->fSize,(w),(h)); } while(0)
#define WKCRect_SetXYWH(r,x,y,w,h) WKCRect_Set(r,x,y,w,h)

/* Float geometry */
#ifndef WKCFloatPoint_Set
#define WKCFloatPoint_Set(p,x,y)         do { (p)->fX=(x); (p)->fY=(y); } while(0)
#endif
#ifndef WKCFloatPoint_SetPoint
#define WKCFloatPoint_SetPoint(dst,src)  do { (dst)->fX=(src)->fX; (dst)->fY=(src)->fY; } while(0)
#endif
#ifndef WKCFloatSize_Set
#define WKCFloatSize_Set(s,w,h)          do { (s)->fWidth=(w); (s)->fHeight=(h); } while(0)
#endif
#ifndef WKCFloatSize_SetSize
#define WKCFloatSize_SetSize(dst,src)    do { (dst)->fWidth=(src)->fWidth; (dst)->fHeight=(src)->fHeight; } while(0)
#endif
#ifndef WKCFloatRect_SetXYWH
#define WKCFloatRect_SetXYWH(r,x,y,w,h) do { (r)->fPoint.fX=(x); (r)->fPoint.fY=(y); (r)->fSize.fWidth=(w); (r)->fSize.fHeight=(h); } while(0)
#endif

/* Min/Max */
#ifndef WKC_MAX
#define WKC_MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef WKC_MIN
#define WKC_MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* WKCFloatRect intersection */
#define WKCFloatRect_Intersects(a,b) \
    ((a)->fPoint.fX < (b)->fPoint.fX+(b)->fSize.fWidth && \
     (a)->fPoint.fX+(a)->fSize.fWidth > (b)->fPoint.fX && \
     (a)->fPoint.fY < (b)->fPoint.fY+(b)->fSize.fHeight && \
     (a)->fPoint.fY+(a)->fSize.fHeight > (b)->fPoint.fY)

#define WKCFloatRect_Intersect(a,b,out) do { \
    float _x1 = (a)->fPoint.fX > (b)->fPoint.fX ? (a)->fPoint.fX : (b)->fPoint.fX; \
    float _y1 = (a)->fPoint.fY > (b)->fPoint.fY ? (a)->fPoint.fY : (b)->fPoint.fY; \
    float _x2 = ((a)->fPoint.fX+(a)->fSize.fWidth) < ((b)->fPoint.fX+(b)->fSize.fWidth) ? ((a)->fPoint.fX+(a)->fSize.fWidth) : ((b)->fPoint.fX+(b)->fSize.fWidth); \
    float _y2 = ((a)->fPoint.fY+(a)->fSize.fHeight) < ((b)->fPoint.fY+(b)->fSize.fHeight) ? ((a)->fPoint.fY+(a)->fSize.fHeight) : ((b)->fPoint.fY+(b)->fSize.fHeight); \
    (out)->fPoint.fX=_x1; (out)->fPoint.fY=_y1; (out)->fSize.fWidth=_x2-_x1; (out)->fSize.fHeight=_y2-_y1; \
} while(0)

#ifdef __cplusplus
struct WKCFileProcs;
struct WKCPasteboardProcs;
#else
typedef struct WKCFileProcs WKCFileProcs;
typedef struct WKCPasteboardProcs WKCPasteboardProcs;
#endif
#endif /* _WKC_BASE_H_ */
