// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import <CoreGraphics/CoreGraphics.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#import <Foundation/Foundation.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>

#include "starboard/common/log.h"
#import "starboard/tvos/shared/media/egl_adapter.h"
#import "starboard/tvos/shared/media/egl_surface.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"

static const EGLDisplay kValidEglDisplay = reinterpret_cast<EGLDisplay>(1);

// Make sure we aren't rewriting glBindFramebuffer in this file, and
// add the declaration for the real method.
#undef glBindFramebuffer
extern "C" {
GL_APICALL void GL_APIENTRY glBindFramebuffer(GLenum target,
                                              GLuint framebuffer);
}  // extern "C"

void eaglBindFramebuffer(GLenum target, GLuint framebuffer) {
  if (framebuffer == 0) {
    @autoreleasepool {
      SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
      framebuffer = eglAdapter.currentDisplayFramebufferID;
    }
  }
  glBindFramebuffer(target, framebuffer);
}

static void SBDEGLAdapterExtractWidthHeightFromParams(const EGLint* params,
                                                      EGLint* width,
                                                      EGLint* height) {
  int i = 0;
  while (params && params[i] != EGL_NONE) {
    EGLint key = params[i];
    EGLint value = params[++i];
    if (key == EGL_WIDTH) {
      *width = value;
    }
    if (key == EGL_HEIGHT) {
      *height = value;
    }
    i++;
  }
  *width = MAX(1, *width);
  *height = MAX(1, *height);
}

#pragma mark - EGL API Implementation

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return kValidEglDisplay;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (dpy != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    if (major) {
      *major = 3;
    }

    if (minor) {
      *minor = 0;
    }

    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return EGL_TRUE;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy,
                           const EGLint* attrib_list,
                           EGLConfig* configs,
                           EGLint config_size,
                           EGLint* num_config) {
  if (configs) {
    configs[0] = reinterpret_cast<void*>(1);
  }
  *num_config = 1;

  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return EGL_TRUE;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy,
                                  EGLConfig config,
                                  EGLNativeWindowType win,
                                  const EGLint* attrib_list) {
  EGLSurface ref;
  @autoreleasepool {
    id<SBDStarboardApplication> application = SBDGetApplication();
    SBDWindowManager* windowManager = application.windowManager;
    CGSize screenSize = windowManager.screenSize;
    EGLint width = screenSize.width;
    EGLint height = screenSize.height;
    SBDEGLAdapterExtractWidthHeightFromParams(attrib_list, &width, &height);

    SBDEglAdapter* eglAdapter = application.eglAdapter;
    SBDEglSurface* surface = [eglAdapter surfaceOfType:SBDEAGLSurfaceTypeWindow
                                                 width:width
                                                height:height];
    ref = [eglAdapter starboardSurfaceForApplicationSurface:surface];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return ref;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy,
                                   EGLConfig config,
                                   const EGLint* attrib_list) {
  EGLSurface ref;
  @autoreleasepool {
    id<SBDStarboardApplication> application = SBDGetApplication();
    SBDWindowManager* windowManager = application.windowManager;
    CGSize screenSize = windowManager.screenSize;
    EGLint width = screenSize.width;
    EGLint height = screenSize.height;
    SBDEGLAdapterExtractWidthHeightFromParams(attrib_list, &width, &height);

    SBDEglAdapter* eglAdapter = application.eglAdapter;
    SBDEglSurface* surface =
        [eglAdapter surfaceOfType:SBDEAGLSurfaceTypeOffscreenSurface
                            width:width
                           height:height];
    ref = [eglAdapter starboardSurfaceForApplicationSurface:surface];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return ref;
}

EGLContext eglCreateContext(EGLDisplay display,
                            EGLConfig config,
                            EGLContext share_context,
                            const EGLint* attrib_list) {
  EGLContext ref;
  @autoreleasepool {
    id<SBDStarboardApplication> application = SBDGetApplication();
    SBDWindowManager* windowManager = application.windowManager;
    CGSize screenSize = windowManager.screenSize;
    EGLint width = screenSize.width;
    EGLint height = screenSize.height;
    SBDEGLAdapterExtractWidthHeightFromParams(attrib_list, &width, &height);

    SBDEglAdapter* eglAdapter = application.eglAdapter;
    EAGLContext* sharedContext;
    if (share_context) {
      sharedContext =
          [eglAdapter applicationContextForStarboardContext:share_context];
    }
    EAGLContext* context = [eglAdapter contextWithSharedContext:sharedContext];
    ref = [eglAdapter starboardContextForApplicationContext:context];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return ref;
}

EGLBoolean eglMakeCurrent(EGLDisplay display,
                          EGLSurface draw,
                          EGLSurface read,
                          EGLContext ctx) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    if (draw != read) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_SURFACE;
      return EGL_FALSE;
    }

    SBDEglSurface* drawSurface =
        [eglAdapter applicationSurfaceForStarboardSurface:draw];
    EAGLContext* context =
        [eglAdapter applicationContextForStarboardContext:ctx];
    [eglAdapter setCurrentContext:context andBindToSurface:drawSurface];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }

  return EGL_TRUE;
}

EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    SBDEglSurface* applicationSurface =
        [eglAdapter applicationSurfaceForStarboardSurface:surface];
    [applicationSurface swapBuffers];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }

  return EGL_TRUE;
}

EGLBoolean eglDestroyContext(EGLDisplay display, EGLContext ctx) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    EAGLContext* context =
        [eglAdapter applicationContextForStarboardContext:ctx];
    [eglAdapter destroyContext:context];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }

  return EGL_TRUE;
}

EGLBoolean eglDestroySurface(EGLDisplay display, EGLSurface sfc) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    SBDEglSurface* surface =
        [eglAdapter applicationSurfaceForStarboardSurface:sfc];
    [eglAdapter destroySurface:surface];
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }

  return EGL_TRUE;
}

EGLint eglGetError(void) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    return eglAdapter.threadLocalError;
  }
}

EGLBoolean eglQuerySurface(EGLDisplay display,
                           EGLSurface surface,
                           EGLint attribute,
                           EGLint* value) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    switch (attribute) {
      case EGL_WIDTH:
        *value = SBDGetApplication().windowManager.screenSize.width;
        break;
      case EGL_HEIGHT:
        *value = SBDGetApplication().windowManager.screenSize.height;
        break;
      default:
        eglAdapter.threadLocalError = EGL_BAD_ATTRIBUTE;
        return EGL_FALSE;
    }

    eglAdapter.threadLocalError = EGL_SUCCESS;
    return EGL_TRUE;
  }
}

EGLBoolean eglSwapInterval(EGLDisplay display, EGLint interval) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    eglAdapter.threadLocalError = EGL_SUCCESS;
    if (interval == 1) {
      // eglSwapBuffers always presents with interval 1.
      return EGL_TRUE;
    } else {
      SB_NOTIMPLEMENTED();
      return EGL_FALSE;
    }
  }
}

EGLBoolean eglTerminate(EGLDisplay display) {
  @autoreleasepool {
    SBDEglAdapter* eglAdapter = SBDGetApplication().eglAdapter;
    if (display != kValidEglDisplay) {
      SB_NOTIMPLEMENTED();
      eglAdapter.threadLocalError = EGL_BAD_DISPLAY;
      return EGL_FALSE;
    }

    // Nothing needs to be done.
    eglAdapter.threadLocalError = EGL_SUCCESS;
  }
  return EGL_TRUE;
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(
    const char* procname) {
  return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
      dlsym(RTLD_DEFAULT, procname));
}

#pragma mark - Unimplemented EGL API Methods

EGLBoolean eglCopyBuffers(EGLDisplay display,
                          EGLSurface surface,
                          EGLNativePixmapType target) {
  SB_NOTIMPLEMENTED();
  return EGL_FALSE;
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy,
                                  EGLConfig config,
                                  EGLNativePixmapType pixmap,
                                  const EGLint* attrib_list) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_SURFACE;
}

EGLImageKHR eglCreateImageKHR(EGLDisplay display,
                              EGLContext ctx,
                              EGLenum target,
                              EGLClientBuffer buffer,
                              const EGLint* attrib_list) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

EGLBoolean eglDestroyImageKHR(EGLDisplay display, EGLImageKHR image) {
  SB_NOTIMPLEMENTED();
  return EGL_FALSE;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy,
                              EGLConfig config,
                              EGLint attribute,
                              EGLint* value) {
  SB_NOTIMPLEMENTED();
  return EGL_FALSE;
}

EGLBoolean eglGetConfigs(EGLDisplay dpy,
                         EGLConfig* configs,
                         EGLint config_size,
                         EGLint* num_config) {
  SB_NOTIMPLEMENTED();
  return EGL_FALSE;
}

EGLDisplay eglGetCurrentDisplay(void) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_DISPLAY;
}

EGLSurface eglGetCurrentSurface(EGLint readdraw) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_SURFACE;
}

EGLBoolean eglQueryContext(EGLDisplay dpy,
                           EGLContext ctx,
                           EGLint attribute,
                           EGLint* value) {
  SB_NOTIMPLEMENTED();
  return false;
}

const char* eglQueryString(EGLDisplay dpy, EGLint name) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

EGLBoolean eglWaitGL(void) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglWaitNative(EGLint engine) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint buffer) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy,
                            EGLSurface surface,
                            EGLint attribute,
                            EGLint value) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglBindAPI(EGLenum api) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLenum eglQueryAPI(void) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
                                            EGLenum buftype,
                                            EGLClientBuffer buffer,
                                            EGLConfig config,
                                            const EGLint* attrib_list) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_SURFACE;
}

EGLBoolean eglReleaseThread(void) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglWaitClient(void) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLContext eglGetCurrentContext(void) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_CONTEXT;
}

EGLSync eglCreateSync(EGLDisplay dpy,
                      EGLenum type,
                      const EGLAttrib* attrib_list) {
  SB_NOTIMPLEMENTED();
  return 0;
}

EGLBoolean eglDestroySync(EGLDisplay dpy, EGLSync sync) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLint eglClientWaitSync(EGLDisplay dpy,
                         EGLSync sync,
                         EGLint flags,
                         EGLTime timeout) {
  SB_NOTIMPLEMENTED();
  return 0;
}

EGLBoolean eglGetSyncAttrib(EGLDisplay dpy,
                            EGLSync sync,
                            EGLint attribute,
                            EGLAttrib* value) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLImage eglCreateImage(EGLDisplay dpy,
                        EGLContext ctx,
                        EGLenum target,
                        EGLClientBuffer buffer,
                        const EGLAttrib* attrib_list) {
  SB_NOTIMPLEMENTED();
  return 0;
}

EGLBoolean eglDestroyImage(EGLDisplay dpy, EGLImage image) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLDisplay eglGetPlatformDisplay(EGLenum platform,
                                 void* native_display,
                                 const EGLAttrib* attrib_list) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_DISPLAY;
}

EGLSurface eglCreatePlatformWindowSurface(EGLDisplay dpy,
                                          EGLConfig config,
                                          void* native_window,
                                          const EGLAttrib* attrib_list) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_SURFACE;
}

EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay dpy,
                                          EGLConfig config,
                                          void* native_pixmap,
                                          const EGLAttrib* attrib_list) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_SURFACE;
}

EGLBoolean eglWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglQuerySurfacePointerANGLE(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLint attribute,
                                       void** value) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglPostSubBufferNV(EGLDisplay dpy,
                              EGLSurface surface,
                              EGLint x,
                              EGLint y,
                              EGLint width,
                              EGLint height) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLDisplay eglGetPlatformDisplayEXT(EGLenum platform,
                                    void* native_display,
                                    const EGLint* attrib_list) {
  SB_NOTIMPLEMENTED();
  return EGL_NO_DISPLAY;
}

EGLBoolean eglQueryDisplayAttribEXT(EGLDisplay dpy,
                                    EGLint attribute,
                                    EGLAttrib* value) {
  SB_NOTIMPLEMENTED();
  return false;
}

EGLBoolean eglQueryDeviceAttribEXT(EGLDeviceEXT device,
                                   EGLint attribute,
                                   EGLAttrib* value) {
  SB_NOTIMPLEMENTED();
  return false;
}

const char* eglQueryDeviceStringEXT(EGLDeviceEXT device, EGLint name) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

EGLDeviceEXT eglCreateDeviceANGLE(EGLint device_type,
                                  void* native_device,
                                  const EGLAttrib* attrib_list) {
  SB_NOTIMPLEMENTED();
  return 0;
}

EGLBoolean eglReleaseDeviceANGLE(EGLDeviceEXT device) {
  SB_NOTIMPLEMENTED();
  return false;
}
