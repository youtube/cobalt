---
layout: doc
title: "Starboard Module Reference: egl.h"
---

The EGL API provides an interface with accompanying type declarations and
defines that together provide a single consistent method of EGL usage across
platforms.

This API is designed to abstract the differences between EGL implementations and
versions on different systems, and to remove the requirement for any other code
to directly pull in and use these system libraries.
EGL Version

This API has the ability to support EGL 1.5, however it is not required to
support anything beyond EGL 1.4. The user is responsible for ensuring that the
functions from EGL 1.5 they are calling from the interface are valid.

## Macros ##

### SB_EGL_ALPHA_FORMAT ###

EGL_VERSION_1_2

### SB_EGL_ALPHA_SIZE ###

EGL_VERSION_1_0

### SB_EGL_BACK_BUFFER ###

EGL_VERSION_1_1

### SB_EGL_CONFORMANT ###

EGL_VERSION_1_3

### SB_EGL_CONTEXT_MAJOR_VERSION ###

EGL_VERSION_1_5

### SB_EGL_DEFAULT_DISPLAY ###

EGL_VERSION_1_4

## Typedefs ##

### SbEglCastsToProperFunctionPointerType ###

The following type definitions were adapted from the types declared in [https://www.khronos.org/registry/EGL/api/EGL/egl.h](https://www.khronos.org/registry/EGL/api/EGL/egl.h)
.

#### Definition ####

```
typedef void(* SbEglCastsToProperFunctionPointerType) (void)
```

### SbEglInt32 ###

The following type definitions were adapted from the types declared in [https://www.khronos.org/registry/EGL/api/EGL/eglplatform.h](https://www.khronos.org/registry/EGL/api/EGL/eglplatform.h)
.

#### Definition ####

```
typedef int32_t SbEglInt32
```

## Structs ##

### SbEglInterface ###

#### Members ####

*   `SbEglBoolean(* eglChooseConfig`
*   `SbEglBoolean(* eglCopyBuffers`
*   `SbEglContext(* eglCreateContext`
*   `SbEglSurface(* eglCreatePbufferSurface`
*   `SbEglSurface(* eglCreatePixmapSurface`
*   `SbEglSurface(* eglCreateWindowSurface`
*   `SbEglBoolean(* eglDestroyContext`
*   `SbEglBoolean(* eglDestroySurface`
*   `SbEglBoolean(* eglGetConfigAttrib`
*   `SbEglBoolean(* eglGetConfigs`
*   `SbEglDisplay(* eglGetCurrentDisplay`
*   `SbEglSurface(* eglGetCurrentSurface`
*   `SbEglDisplay(* eglGetDisplay`
*   `SbEglInt32(* eglGetError`
*   `SbEglCastsToProperFunctionPointerType(* eglGetProcAddress`
*   `SbEglBoolean(* eglInitialize`
*   `SbEglBoolean(* eglMakeCurrent`
*   `SbEglBoolean(* eglQueryContext`
*   `const char *(* eglQueryString`
*   `SbEglBoolean(* eglQuerySurface`
*   `SbEglBoolean(* eglSwapBuffers`
*   `SbEglBoolean(* eglTerminate`
*   `SbEglBoolean(* eglWaitGL`
*   `SbEglBoolean(* eglWaitNative`
*   `SbEglBoolean(* eglBindTexImage`
*   `SbEglBoolean(* eglReleaseTexImage`
*   `SbEglBoolean(* eglSurfaceAttrib`
*   `SbEglBoolean(* eglSwapInterval`
*   `SbEglBoolean(* eglBindAPI`
*   `SbEglEnum(* eglQueryAPI`
*   `SbEglSurface(* eglCreatePbufferFromClientBuffer`
*   `SbEglBoolean(* eglReleaseThread`
*   `SbEglBoolean(* eglWaitClient`
*   `SbEglContext(* eglGetCurrentContext`
*   `SbEglSync(* eglCreateSync`
*   `SbEglBoolean(* eglDestroySync`
*   `SbEglInt32(* eglClientWaitSync`
*   `SbEglBoolean(* eglGetSyncAttrib`
*   `SbEglImage(* eglCreateImage`
*   `SbEglBoolean(* eglDestroyImage`
*   `SbEglDisplay(* eglGetPlatformDisplay`
*   `SbEglSurface(* eglCreatePlatformWindowSurface`
*   `SbEglSurface(* eglCreatePlatformPixmapSurface`
*   `SbEglBoolean(* eglWaitSync`

