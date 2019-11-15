/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrTypes_DEFINED
#define GrTypes_DEFINED

#include "include/core/SkMath.h"
#include "include/core/SkTypes.h"
#include "include/gpu/GrConfig.h"

class GrBackendSemaphore;
class SkImage;
class SkSurface;

////////////////////////////////////////////////////////////////////////////////

/**
 * Defines overloaded bitwise operators to make it easier to use an enum as a
 * bitfield.
 */
#define GR_MAKE_BITFIELD_OPS(X) \
    inline X operator |(X a, X b) { \
        return (X) (+a | +b); \
    } \
    inline X& operator |=(X& a, X b) { \
        return (a = a | b); \
    } \
    inline X operator &(X a, X b) { \
        return (X) (+a & +b); \
    } \
    inline X& operator &=(X& a, X b) { \
        return (a = a & b); \
    } \
    template <typename T> \
    inline X operator &(T a, X b) { \
        return (X) (+a & +b); \
    } \
    template <typename T> \
    inline X operator &(X a, T b) { \
        return (X) (+a & +b); \
    } \

#define GR_DECL_BITFIELD_OPS_FRIENDS(X) \
    friend X operator |(X a, X b); \
    friend X& operator |=(X& a, X b); \
    \
    friend X operator &(X a, X b); \
    friend X& operator &=(X& a, X b); \
    \
    template <typename T> \
    friend X operator &(T a, X b); \
    \
    template <typename T> \
    friend X operator &(X a, T b); \

/**
 * Wraps a C++11 enum that we use as a bitfield, and enables a limited amount of
 * masking with type safety. Instantiated with the ~ operator.
 */
template<typename TFlags> class GrTFlagsMask {
public:
    constexpr explicit GrTFlagsMask(TFlags value) : GrTFlagsMask(static_cast<int>(value)) {}
    constexpr explicit GrTFlagsMask(int value) : fValue(value) {}
    constexpr int value() const { return fValue; }
private:
    const int fValue;
};

// Or-ing a mask always returns another mask.
template<typename TFlags> constexpr GrTFlagsMask<TFlags> operator|(GrTFlagsMask<TFlags> a,
                                                                   GrTFlagsMask<TFlags> b) {
    return GrTFlagsMask<TFlags>(a.value() | b.value());
}
template<typename TFlags> constexpr GrTFlagsMask<TFlags> operator|(GrTFlagsMask<TFlags> a,
                                                                   TFlags b) {
    return GrTFlagsMask<TFlags>(a.value() | static_cast<int>(b));
}
template<typename TFlags> constexpr GrTFlagsMask<TFlags> operator|(TFlags a,
                                                                   GrTFlagsMask<TFlags> b) {
    return GrTFlagsMask<TFlags>(static_cast<int>(a) | b.value());
}
template<typename TFlags> inline GrTFlagsMask<TFlags>& operator|=(GrTFlagsMask<TFlags>& a,
                                                                  GrTFlagsMask<TFlags> b) {
    return (a = a | b);
}

// And-ing two masks returns another mask; and-ing one with regular flags returns flags.
template<typename TFlags> constexpr GrTFlagsMask<TFlags> operator&(GrTFlagsMask<TFlags> a,
                                                                   GrTFlagsMask<TFlags> b) {
    return GrTFlagsMask<TFlags>(a.value() & b.value());
}
template<typename TFlags> constexpr TFlags operator&(GrTFlagsMask<TFlags> a, TFlags b) {
    return static_cast<TFlags>(a.value() & static_cast<int>(b));
}
template<typename TFlags> constexpr TFlags operator&(TFlags a, GrTFlagsMask<TFlags> b) {
    return static_cast<TFlags>(static_cast<int>(a) & b.value());
}
template<typename TFlags> inline TFlags& operator&=(TFlags& a, GrTFlagsMask<TFlags> b) {
    return (a = a & b);
}

/**
 * Defines bitwise operators that make it possible to use an enum class as a
 * basic bitfield.
 */
#define GR_MAKE_BITFIELD_CLASS_OPS(X) \
    constexpr GrTFlagsMask<X> operator~(X a) { \
        return GrTFlagsMask<X>(~static_cast<int>(a)); \
    } \
    constexpr X operator|(X a, X b) { \
        return static_cast<X>(static_cast<int>(a) | static_cast<int>(b)); \
    } \
    inline X& operator|=(X& a, X b) { \
        return (a = a | b); \
    } \
    constexpr bool operator&(X a, X b) { \
        return SkToBool(static_cast<int>(a) & static_cast<int>(b)); \
    } \

#define GR_DECL_BITFIELD_CLASS_OPS_FRIENDS(X) \
    friend constexpr GrTFlagsMask<X> operator ~(X); \
    friend constexpr X operator |(X, X); \
    friend X& operator |=(X&, X); \
    friend constexpr bool operator &(X, X)

////////////////////////////////////////////////////////////////////////////////

// compile time versions of min/max
#define GR_CT_MAX(a, b) (((b) < (a)) ? (a) : (b))
#define GR_CT_MIN(a, b) (((b) < (a)) ? (b) : (a))

/**
 *  divide, rounding up
 */
static inline constexpr int32_t GrIDivRoundUp(int x, int y) {
    SkASSERT(y > 0);
    return (x + (y-1)) / y;
}
static inline constexpr uint32_t GrUIDivRoundUp(uint32_t x, uint32_t y) {
    return (x + (y-1)) / y;
}
static inline constexpr size_t GrSizeDivRoundUp(size_t x, size_t y) { return (x + (y - 1)) / y; }

/**
 *  align up
 */
static inline constexpr uint32_t GrUIAlignUp(uint32_t x, uint32_t alignment) {
    return GrUIDivRoundUp(x, alignment) * alignment;
}
static inline constexpr size_t GrSizeAlignUp(size_t x, size_t alignment) {
    return GrSizeDivRoundUp(x, alignment) * alignment;
}

/**
 * amount of pad needed to align up
 */
static inline constexpr uint32_t GrUIAlignUpPad(uint32_t x, uint32_t alignment) {
    return (alignment - x % alignment) % alignment;
}
static inline constexpr size_t GrSizeAlignUpPad(size_t x, size_t alignment) {
    return (alignment - x % alignment) % alignment;
}

/**
 *  align down
 */
static inline constexpr uint32_t GrUIAlignDown(uint32_t x, uint32_t alignment) {
    return (x / alignment) * alignment;
}
static inline constexpr size_t GrSizeAlignDown(size_t x, uint32_t alignment) {
    return (x / alignment) * alignment;
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Possible 3D APIs that may be used by Ganesh.
 */
enum class GrBackendApi : unsigned {
    kMetal,
    kDawn,
    kOpenGL,
    kVulkan,
    /**
     * Mock is a backend that does not draw anything. It is used for unit tests
     * and to measure CPU overhead.
     */
    kMock,

    /**
     * Added here to support the legacy GrBackend enum value and clients who referenced it using
     * GrBackend::kOpenGL_GrBackend.
     */
    kOpenGL_GrBackend = kOpenGL,
};
<<<<<<< HEAD
static const int kGrPixelConfigCnt = kLast_GrPixelConfig + 1;

// Aliases for pixel configs that match skia's byte order.
#if SK_PMCOLOR_BYTE_ORDER(B,G,R,A)
    static const GrPixelConfig kSkia8888_GrPixelConfig = kBGRA_8888_GrPixelConfig;
#elif SK_PMCOLOR_BYTE_ORDER(R,G,B,A)
    static const GrPixelConfig kSkia8888_GrPixelConfig = kRGBA_8888_GrPixelConfig;
#else
    #error "SK_*32_SHIFT values must correspond to GL_BGRA or GL_RGBA format."
#endif

// Returns true if the pixel config is 32 bits per pixel
static inline bool GrPixelConfigIs8888Unorm(GrPixelConfig config) {
    switch (config) {
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
            return true;
        case kUnknown_GrPixelConfig:
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kRG_float_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
            return false;
    }
    SkFAIL("Invalid pixel config");
    return false;
}

// Returns true if the color (non-alpha) components represent sRGB values. It does NOT indicate that
// all three color components are present in the config or anything about their order.
static inline bool GrPixelConfigIsSRGB(GrPixelConfig config) {
    switch (config) {
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
            return true;
        case kUnknown_GrPixelConfig:
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kRG_float_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
            return false;
    }
    SkFAIL("Invalid pixel config");
    return false;
}

// Takes a config and returns the equivalent config with the R and B order
// swapped if such a config exists. Otherwise, kUnknown_GrPixelConfig
static inline GrPixelConfig GrPixelConfigSwapRAndB(GrPixelConfig config) {
    switch (config) {
        case kBGRA_8888_GrPixelConfig:
            return kRGBA_8888_GrPixelConfig;
        case kRGBA_8888_GrPixelConfig:
            return kBGRA_8888_GrPixelConfig;
        case kSBGRA_8888_GrPixelConfig:
            return kSRGBA_8888_GrPixelConfig;
        case kSRGBA_8888_GrPixelConfig:
            return kSBGRA_8888_GrPixelConfig;
        case kUnknown_GrPixelConfig:
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kRG_float_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
            return kUnknown_GrPixelConfig;
    }
    SkFAIL("Invalid pixel config");
    return kUnknown_GrPixelConfig;
}

static inline size_t GrBytesPerPixel(GrPixelConfig config) {
    switch (config) {
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
            return 1;
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
            return 2;
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
            return 4;
        case kRGBA_half_GrPixelConfig:
            return 8;
        case kRGBA_float_GrPixelConfig:
            return 16;
        case kRG_float_GrPixelConfig:
            return 8;
        case kUnknown_GrPixelConfig:
            return 0;
    }
    SkFAIL("Invalid pixel config");
    return 0;
}

static inline bool GrPixelConfigIsOpaque(GrPixelConfig config) {
    switch (config) {
        case kRGB_565_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRG_float_GrPixelConfig:
            return true;
        case kAlpha_8_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kUnknown_GrPixelConfig:
            return false;
    }
    SkFAIL("Invalid pixel config");
    return false;
}

static inline bool GrPixelConfigIsAlphaOnly(GrPixelConfig config) {
    switch (config) {
        case kAlpha_8_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
            return true;
        case kUnknown_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kRG_float_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
            return false;
    }
    SkFAIL("Invalid pixel config.");
    return false;
}

static inline bool GrPixelConfigIsFloatingPoint(GrPixelConfig config) {
    switch (config) {
        case kRGBA_float_GrPixelConfig:
        case kRG_float_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
            return true;
        case kUnknown_GrPixelConfig:
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
            return false;
    }
    SkFAIL("Invalid pixel config");
    return false;
}

static inline bool GrPixelConfigIsSint(GrPixelConfig config) {
    return config == kRGBA_8888_sint_GrPixelConfig;
}

static inline bool GrPixelConfigIsUnorm(GrPixelConfig config) {
    switch (config) {
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kSBGRA_8888_GrPixelConfig:
            return true;
        case kUnknown_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_8888_sint_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kRG_float_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
            return false;
    }
    SkFAIL("Invalid pixel config.");
    return false;
}
=======
>>>>>>> acc9e0a2d6f04288dc1f1596570ce7306a790ced

/**
 * Previously the above enum was not an enum class but a normal enum. To support the legacy use of
 * the enum values we define them below so that no clients break.
 */
typedef GrBackendApi GrBackend;

static constexpr GrBackendApi kMetal_GrBackend = GrBackendApi::kMetal;
static constexpr GrBackendApi kVulkan_GrBackend = GrBackendApi::kVulkan;
static constexpr GrBackendApi kMock_GrBackend = GrBackendApi::kMock;

///////////////////////////////////////////////////////////////////////////////

/**
 * Used to say whether a texture has mip levels allocated or not.
 */
enum class GrMipMapped : bool {
    kNo = false,
    kYes = true
};

/*
 * Can a GrBackendObject be rendered to?
 */
enum class GrRenderable : bool {
    kNo = false,
    kYes = true
};

/*
 * Used to say whether texture is backed by protected memory.
 */
enum class GrProtected : bool {
    kNo = false,
    kYes = true
};

///////////////////////////////////////////////////////////////////////////////

/**
 * GPU SkImage and SkSurfaces can be stored such that (0, 0) in texture space may correspond to
 * either the top-left or bottom-left content pixel.
 */
enum GrSurfaceOrigin : int {
    kTopLeft_GrSurfaceOrigin,
    kBottomLeft_GrSurfaceOrigin,
};

/**
 * A GrContext's cache of backend context state can be partially invalidated.
 * These enums are specific to the GL backend and we'd add a new set for an alternative backend.
 */
enum GrGLBackendState {
    kRenderTarget_GrGLBackendState     = 1 << 0,
    // Also includes samplers bound to texture units.
    kTextureBinding_GrGLBackendState   = 1 << 1,
    // View state stands for scissor and viewport
    kView_GrGLBackendState             = 1 << 2,
    kBlend_GrGLBackendState            = 1 << 3,
    kMSAAEnable_GrGLBackendState       = 1 << 4,
    kVertex_GrGLBackendState           = 1 << 5,
    kStencil_GrGLBackendState          = 1 << 6,
    kPixelStore_GrGLBackendState       = 1 << 7,
    kProgram_GrGLBackendState          = 1 << 8,
    kFixedFunction_GrGLBackendState    = 1 << 9,
    kMisc_GrGLBackendState             = 1 << 10,
    kPathRendering_GrGLBackendState    = 1 << 11,
    kALL_GrGLBackendState              = 0xffff
};

/**
 * This value translates to reseting all the context state for any backend.
 */
static const uint32_t kAll_GrBackendState = 0xffffffff;

enum GrFlushFlags {
    kNone_GrFlushFlags = 0,
    // flush will wait till all submitted GPU work is finished before returning.
    kSyncCpu_GrFlushFlag = 0x1,
};

typedef void* GrGpuFinishedContext;
typedef void (*GrGpuFinishedProc)(GrGpuFinishedContext finishedContext);

/**
 * Struct to supply options to flush calls.
 *
 * After issuing all commands, fNumSemaphore semaphores will be signaled by the gpu. The client
 * passes in an array of fNumSemaphores GrBackendSemaphores. In general these GrBackendSemaphore's
 * can be either initialized or not. If they are initialized, the backend uses the passed in
 * semaphore. If it is not initialized, a new semaphore is created and the GrBackendSemaphore
 * object is initialized with that semaphore.
 *
 * The client will own and be responsible for deleting the underlying semaphores that are stored
 * and returned in initialized GrBackendSemaphore objects. The GrBackendSemaphore objects
 * themselves can be deleted as soon as this function returns.
 *
 * If a finishedProc is provided, the finishedProc will be called when all work submitted to the gpu
 * from this flush call and all previous flush calls has finished on the GPU. If the flush call
 * fails due to an error and nothing ends up getting sent to the GPU, the finished proc is called
 * immediately.
 */
struct GrFlushInfo {
    GrFlushFlags         fFlags = kNone_GrFlushFlags;
    int                  fNumSemaphores = 0;
    GrBackendSemaphore*  fSignalSemaphores = nullptr;
    GrGpuFinishedProc    fFinishedProc = nullptr;
    GrGpuFinishedContext fFinishedContext = nullptr;
};

/**
 * Enum used as return value when flush with semaphores so the client knows whether the semaphores
 * were submitted to GPU or not.
 */
enum class GrSemaphoresSubmitted : bool {
    kNo = false,
    kYes = true
};

/**
 * Array of SkImages and SkSurfaces which Skia will prepare for external use when passed into a
 * flush call on GrContext. All the SkImages and SkSurfaces must be GPU backed.
 *
 * If fPrepareSurfaceForPresent is not nullptr, then it must be an array the size of fNumSurfaces.
 * Each entry in the array corresponds to the SkSurface at the same index in the fSurfaces array. If
 * an entry is true, then that surface will be prepared for both external use and present.
 *
 * Currently this only has an effect if the backend API is Vulkan. In this case, all the underlying
 * VkImages associated with the SkImages and SkSurfaces will be transitioned into the VkQueueFamily
 * in which they were originally wrapped or created with. This allows a client to wrap a VkImage
 * from a queue which is different from the graphics queue and then have Skia transition it back to
 * that queue without needing to delete the SkImage or SkSurface. If the an SkSurface is also
 * flagged to be prepared for present, then its VkImageLayout will be set to
 * VK_IMAGE_LAYOUT_PRESENT_SRC_KHR if the VK_KHR_swapchain extension has been enabled for the
 * GrContext and the original queue is not VK_QUEUE_FAMILY_EXTERNAL or VK_QUEUE_FAMILY_FOREIGN_EXT.
 *
 * If an SkSurface or SkImage is used again, it will be transitioned back to the graphics queue and
 * whatever layout is needed for its use.
 */
struct GrPrepareForExternalIORequests {
    int fNumImages = 0;
    SkImage** fImages = nullptr;
    int fNumSurfaces = 0;
    SkSurface** fSurfaces = nullptr;
    bool* fPrepareSurfaceForPresent = nullptr;

    bool hasRequests() const { return fNumImages || fNumSurfaces; }
};

#endif
