//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsEGLDL.cpp: Implements the FunctionsEGLDL class.

#include "libANGLE/renderer/gl/egl/FunctionsEGLDL.h"

#include <dlfcn.h>

namespace rx
{
namespace
{
#if defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
// Hermetic Starboard (modular and Evergreen) builds can't dlopen() the
// platform's EGL library. The embedder provides the native EGL/GLES entry
// points instead (see ANGLESetNativeEGLGetProcAddress()).
using NativeEGLGetProcAddressProc = void *(*)(const char *name);
NativeEGLGetProcAddressProc nativeEGLGetProcAddress;
#else
// In ideal world, we would want this to be a member of FunctionsEGLDL,
// and call dlclose() on it in ~FunctionsEGLDL().
// However, some GL implementations are broken and don't allow multiple
// load/unload cycles, but only static linking with them.
// That's why we dlopen() this handle once and never dlclose() it.
// This is consistent with Chromium's CleanupNativeLibraries() code,
// referencing crbug.com/250813 and http://www.xfree86.org/4.3.0/DRI11.html
void *nativeEGLHandle;
#endif  // defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
}  // anonymous namespace

#if !defined(STARBOARD) || defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
FunctionsEGLDL::FunctionsEGLDL() : mGetProcAddressPtr(nullptr) {}
#else
FunctionsEGLDL::FunctionsEGLDL() = default;
#endif

FunctionsEGLDL::~FunctionsEGLDL() {}

egl::Error FunctionsEGLDL::initialize(EGLAttrib platformType,
                                      EGLNativeDisplayType nativeDisplay,
                                      const char *libName,
                                      void *eglHandle)
{
#if defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
    if (!nativeEGLGetProcAddress)
    {
        return egl::Error(EGL_NOT_INITIALIZED,
                          "No native EGL entry points (ANGLESetNativeEGLGetProcAddress)");
    }
    // getProcAddress() calls nativeEGLGetProcAddress directly, through its own
    // type.
    return FunctionsEGL::initialize(platformType, nativeDisplay);
#else

    if (eglHandle)
    {
        // If the handle is provided, use it.
        // Caller has already dlopened the vendor library.
        nativeEGLHandle = eglHandle;
    }

    if (!nativeEGLHandle)
    {
        nativeEGLHandle = dlopen(libName, RTLD_NOW);
        if (!nativeEGLHandle)
        {
            std::ostringstream err;
            err << "Could not dlopen native EGL: " << dlerror();
            return egl::Error(EGL_NOT_INITIALIZED, err.str());
        }
    }

    mGetProcAddressPtr =
        reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(dlsym(nativeEGLHandle, "eglGetProcAddress"));
    if (!mGetProcAddressPtr)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not find eglGetProcAddress");
    }

    return FunctionsEGL::initialize(platformType, nativeDisplay);
#endif  // defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
}

void *FunctionsEGLDL::getProcAddress(const char *name) const
{
#if defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
    return nativeEGLGetProcAddress ? nativeEGLGetProcAddress(name) : nullptr;
#else
    void *f = reinterpret_cast<void *>(mGetProcAddressPtr(name));
    if (f)
    {
        return f;
    }
    return dlsym(nativeEGLHandle, name);
#endif  // defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
}

}  // namespace rx

#if defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
// Sets the function the EGL back-end uses to look up native EGL and GLES entry
// points, instead of dlopen()ing the native EGL library. Must be called before
// the first ANGLE EGL display is initialized.
extern "C" void ANGLESetNativeEGLGetProcAddress(void *(*getProcAddress)(const char *name))
{
    rx::nativeEGLGetProcAddress = getProcAddress;
}
#endif  // defined(STARBOARD) && !defined(ENABLE_BUILDFLAG_IS_STARBOARD_TOOLCHAIN)
