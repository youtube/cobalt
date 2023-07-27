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

## EGL Version ##

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

*   `SbEglBoolean(*eglChooseConfig)(SbEglDisplay dpy, const SbEglInt32
    *attrib_list, SbEglConfig *configs, SbEglInt32 config_size, SbEglInt32
    *num_config)`
*   `SbEglBoolean(*eglCopyBuffers)(SbEglDisplay dpy, SbEglSurface surface,
    SbEglNativePixmapType target)`
*   `SbEglContext(*eglCreateContext)(SbEglDisplay dpy, SbEglConfig config,
    SbEglContext share_context, const SbEglInt32 *attrib_list)`
*   `SbEglSurface(*eglCreatePbufferSurface)(SbEglDisplay dpy, SbEglConfig
    config, const SbEglInt32 *attrib_list)`
*   `SbEglSurface(*eglCreatePixmapSurface)(SbEglDisplay dpy, SbEglConfig config,
    SbEglNativePixmapType pixmap, const SbEglInt32 *attrib_list)`
*   `SbEglSurface(*eglCreateWindowSurface)(SbEglDisplay dpy, SbEglConfig config,
    SbEglNativeWindowType win, const SbEglInt32 *attrib_list)`
*   `SbEglBoolean(*eglDestroyContext)(SbEglDisplay dpy, SbEglContext ctx)`
*   `SbEglBoolean(*eglDestroySurface)(SbEglDisplay dpy, SbEglSurface surface)`
*   `SbEglBoolean(*eglGetConfigAttrib)(SbEglDisplay dpy, SbEglConfig config,
    SbEglInt32 attribute, SbEglInt32 *value)`
*   `SbEglBoolean(*eglGetConfigs)(SbEglDisplay dpy, SbEglConfig *configs,
    SbEglInt32 config_size, SbEglInt32 *num_config)`
*   `SbEglDisplay(*eglGetCurrentDisplay)(void)`
*   `SbEglSurface(*eglGetCurrentSurface)(SbEglInt32 readdraw)`
*   `SbEglDisplay(*eglGetDisplay)(SbEglNativeDisplayType display_id)`
*   `SbEglInt32(*eglGetError)(void)`
*   `SbEglCastsToProperFunctionPointerType(*eglGetProcAddress)(const char
    *procname)`
*   `SbEglBoolean(*eglInitialize)(SbEglDisplay dpy, SbEglInt32 *major,
    SbEglInt32 *minor)`
*   `SbEglBoolean(*eglMakeCurrent)(SbEglDisplay dpy, SbEglSurface draw,
    SbEglSurface read, SbEglContext ctx)`
*   `SbEglBoolean(*eglQueryContext)(SbEglDisplay dpy, SbEglContext ctx,
    SbEglInt32 attribute, SbEglInt32 *value)`
*   `const char *(*eglQueryString)(SbEglDisplay dpy, SbEglInt32 name)`
*   `SbEglBoolean(*eglQuerySurface)(SbEglDisplay dpy, SbEglSurface surface,
    SbEglInt32 attribute, SbEglInt32 *value)`
*   `SbEglBoolean(*eglSwapBuffers)(SbEglDisplay dpy, SbEglSurface surface)`
*   `SbEglBoolean(*eglTerminate)(SbEglDisplay dpy)`
*   `SbEglBoolean(*eglWaitGL)(void)`
*   `SbEglBoolean(*eglWaitNative)(SbEglInt32 engine)`
*   `SbEglBoolean(*eglBindTexImage)(SbEglDisplay dpy, SbEglSurface surface,
    SbEglInt32 buffer)`
*   `SbEglBoolean(*eglReleaseTexImage)(SbEglDisplay dpy, SbEglSurface surface,
    SbEglInt32 buffer)`
*   `SbEglBoolean(*eglSurfaceAttrib)(SbEglDisplay dpy, SbEglSurface surface,
    SbEglInt32 attribute, SbEglInt32 value)`
*   `SbEglBoolean(*eglSwapInterval)(SbEglDisplay dpy, SbEglInt32 interval)`
*   `SbEglBoolean(*eglBindAPI)(SbEglEnum api)`
*   `SbEglEnum(*eglQueryAPI)(void)`
*   `SbEglSurface(*eglCreatePbufferFromClientBuffer)(SbEglDisplay dpy, SbEglEnum
    buftype, SbEglClientBuffer buffer, SbEglConfig config, const SbEglInt32
    *attrib_list)`
*   `SbEglBoolean(*eglReleaseThread)(void)`
*   `SbEglBoolean(*eglWaitClient)(void)`
*   `SbEglContext(*eglGetCurrentContext)(void)`
*   `SbEglSync(*eglCreateSync)(SbEglDisplay dpy, SbEglEnum type, const
    SbEglAttrib *attrib_list)`
*   `SbEglBoolean(*eglDestroySync)(SbEglDisplay dpy, SbEglSync sync)`
*   `SbEglInt32(*eglClientWaitSync)(SbEglDisplay dpy, SbEglSync sync, SbEglInt32
    flags, SbEglTime timeout)`
*   `SbEglBoolean(*eglGetSyncAttrib)(SbEglDisplay dpy, SbEglSync sync,
    SbEglInt32 attribute, SbEglAttrib *value)`
*   `SbEglImage(*eglCreateImage)(SbEglDisplay dpy, SbEglContext ctx, SbEglEnum
    target, SbEglClientBuffer buffer, const SbEglAttrib *attrib_list)`
*   `SbEglBoolean(*eglDestroyImage)(SbEglDisplay dpy, SbEglImage image)`
*   `SbEglDisplay(*eglGetPlatformDisplay)(SbEglEnum platform, void
    *native_display, const SbEglAttrib *attrib_list)`
*   `SbEglSurface(*eglCreatePlatformWindowSurface)(SbEglDisplay dpy, SbEglConfig
    config, void *native_window, const SbEglAttrib *attrib_list)`
*   `SbEglSurface(*eglCreatePlatformPixmapSurface)(SbEglDisplay dpy, SbEglConfig
    config, void *native_pixmap, const SbEglAttrib *attrib_list)`
*   `SbEglBoolean(*eglWaitSync)(SbEglDisplay dpy, SbEglSync sync, SbEglInt32
    flags)`
