// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard EGL API
//
// The EGL API provides an interface with accompanying type declarations and
// defines that together provide a single consistent method of EGL usage across
// platforms.
//
// This API is designed to abstract the differences between EGL implementations
// and versions on different systems, and to remove the requirement for any
// other code to directly pull in and use these system libraries.
//
// # EGL Version
//
// This API has the ability to support EGL 1.5, however it is not required to
// support anything beyond EGL 1.4. The user is responsible for ensuring that
// the functions from EGL 1.5 they are calling from the interface are valid.

#ifndef STARBOARD_EGL_H_
#define STARBOARD_EGL_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/log.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The following type definitions were adapted from the types declared in
// https://www.khronos.org/registry/EGL/api/EGL/eglplatform.h.
typedef int32_t  SbEglInt32;
typedef intptr_t SbEglNativeDisplayType;
typedef intptr_t SbEglNativePixmapType;
typedef intptr_t SbEglNativeWindowType;

// The following type definitions were adapted from the types declared in
// https://www.khronos.org/registry/EGL/api/EGL/egl.h.
typedef void   (*SbEglCastsToProperFunctionPointerType)(void);
typedef int64_t  SbEglAttrib;
typedef uint32_t SbEglBoolean;
typedef void*    SbEglClientBuffer;
typedef void*    SbEglConfig;
typedef void*    SbEglContext;
typedef void*    SbEglDisplay;
typedef uint32_t SbEglEnum;
typedef void*    SbEglImage;
typedef void*    SbEglSurface;
typedef void*    SbEglSync;
typedef uint64_t SbEglTime;

typedef struct SbEglInterface {
  SbEglBoolean (*eglChooseConfig)(SbEglDisplay dpy,
                                  const SbEglInt32* attrib_list,
                                  SbEglConfig* configs,
                                  SbEglInt32 config_size,
                                  SbEglInt32* num_config);
  SbEglBoolean (*eglCopyBuffers)(SbEglDisplay dpy,
                                 SbEglSurface surface,
                                 SbEglNativePixmapType target);
  SbEglContext (*eglCreateContext)(SbEglDisplay dpy,
                                   SbEglConfig config,
                                   SbEglContext share_context,
                                   const SbEglInt32* attrib_list);
  SbEglSurface (*eglCreatePbufferSurface)(SbEglDisplay dpy,
                                          SbEglConfig config,
                                          const SbEglInt32* attrib_list);
  SbEglSurface (*eglCreatePixmapSurface)(SbEglDisplay dpy,
                                         SbEglConfig config,
                                         SbEglNativePixmapType pixmap,
                                         const SbEglInt32* attrib_list);
  SbEglSurface (*eglCreateWindowSurface)(SbEglDisplay dpy,
                                         SbEglConfig config,
                                         SbEglNativeWindowType win,
                                         const SbEglInt32* attrib_list);
  SbEglBoolean (*eglDestroyContext)(SbEglDisplay dpy, SbEglContext ctx);
  SbEglBoolean (*eglDestroySurface)(SbEglDisplay dpy, SbEglSurface surface);
  SbEglBoolean (*eglGetConfigAttrib)(SbEglDisplay dpy,
                                     SbEglConfig config,
                                     SbEglInt32 attribute,
                                     SbEglInt32* value);
  SbEglBoolean (*eglGetConfigs)(SbEglDisplay dpy,
                                SbEglConfig* configs,
                                SbEglInt32 config_size,
                                SbEglInt32* num_config);
  SbEglDisplay (*eglGetCurrentDisplay)(void);
  SbEglSurface (*eglGetCurrentSurface)(SbEglInt32 readdraw);
  SbEglDisplay (*eglGetDisplay)(SbEglNativeDisplayType display_id);
  SbEglInt32 (*eglGetError)(void);
  SbEglCastsToProperFunctionPointerType (*eglGetProcAddress)(
      const char* procname);
  SbEglBoolean (*eglInitialize)(SbEglDisplay dpy,
                                SbEglInt32* major,
                                SbEglInt32* minor);
  SbEglBoolean (*eglMakeCurrent)(SbEglDisplay dpy,
                                 SbEglSurface draw,
                                 SbEglSurface read,
                                 SbEglContext ctx);
  SbEglBoolean (*eglQueryContext)(SbEglDisplay dpy,
                                  SbEglContext ctx,
                                  SbEglInt32 attribute,
                                  SbEglInt32* value);
  const char* (*eglQueryString)(SbEglDisplay dpy, SbEglInt32 name);
  SbEglBoolean (*eglQuerySurface)(SbEglDisplay dpy,
                                  SbEglSurface surface,
                                  SbEglInt32 attribute,
                                  SbEglInt32* value);
  SbEglBoolean (*eglSwapBuffers)(SbEglDisplay dpy, SbEglSurface surface);
  SbEglBoolean (*eglTerminate)(SbEglDisplay dpy);
  SbEglBoolean (*eglWaitGL)(void);
  SbEglBoolean (*eglWaitNative)(SbEglInt32 engine);
  SbEglBoolean (*eglBindTexImage)(SbEglDisplay dpy,
                                  SbEglSurface surface,
                                  SbEglInt32 buffer);
  SbEglBoolean (*eglReleaseTexImage)(SbEglDisplay dpy,
                                     SbEglSurface surface,
                                     SbEglInt32 buffer);
  SbEglBoolean (*eglSurfaceAttrib)(SbEglDisplay dpy,
                                   SbEglSurface surface,
                                   SbEglInt32 attribute,
                                   SbEglInt32 value);
  SbEglBoolean (*eglSwapInterval)(SbEglDisplay dpy, SbEglInt32 interval);
  SbEglBoolean (*eglBindAPI)(SbEglEnum api);
  SbEglEnum (*eglQueryAPI)(void);
  SbEglSurface (*eglCreatePbufferFromClientBuffer)(
      SbEglDisplay dpy,
      SbEglEnum buftype,
      SbEglClientBuffer buffer,
      SbEglConfig config,
      const SbEglInt32* attrib_list);
  SbEglBoolean (*eglReleaseThread)(void);
  SbEglBoolean (*eglWaitClient)(void);
  SbEglContext (*eglGetCurrentContext)(void);
  SbEglSync (*eglCreateSync)(SbEglDisplay dpy,
                             SbEglEnum type,
                             const SbEglAttrib* attrib_list);
  SbEglBoolean (*eglDestroySync)(SbEglDisplay dpy, SbEglSync sync);
  SbEglInt32 (*eglClientWaitSync)(SbEglDisplay dpy,
                                  SbEglSync sync,
                                  SbEglInt32 flags,
                                  SbEglTime timeout);
  SbEglBoolean (*eglGetSyncAttrib)(SbEglDisplay dpy,
                                   SbEglSync sync,
                                   SbEglInt32 attribute,
                                   SbEglAttrib* value);
  SbEglImage (*eglCreateImage)(SbEglDisplay dpy,
                               SbEglContext ctx,
                               SbEglEnum target,
                               SbEglClientBuffer buffer,
                               const SbEglAttrib* attrib_list);
  SbEglBoolean (*eglDestroyImage)(SbEglDisplay dpy, SbEglImage image);
  SbEglDisplay (*eglGetPlatformDisplay)(SbEglEnum platform,
                                        void* native_display,
                                        const SbEglAttrib* attrib_list);
  SbEglSurface (*eglCreatePlatformWindowSurface)(
      SbEglDisplay dpy,
      SbEglConfig config,
      void* native_window,
      const SbEglAttrib* attrib_list);
  SbEglSurface (*eglCreatePlatformPixmapSurface)(
      SbEglDisplay dpy,
      SbEglConfig config,
      void* native_pixmap,
      const SbEglAttrib* attrib_list);
  SbEglBoolean (*eglWaitSync)(SbEglDisplay dpy,
                              SbEglSync sync,
                              SbEglInt32 flags);
} SbEglInterface;

SB_EXPORT const SbEglInterface* SbGetEglInterface();

// All of the following were previously defined in
// https://www.khronos.org/registry/EGL/api/EGL/egl.h.

// EGL_VERSION_1_0
#define SB_EGL_ALPHA_SIZE 0x3021
#define SB_EGL_BAD_ACCESS 0x3002
#define SB_EGL_BAD_ALLOC 0x3003
#define SB_EGL_BAD_ATTRIBUTE 0x3004
#define SB_EGL_BAD_CONFIG 0x3005
#define SB_EGL_BAD_CONTEXT 0x3006
#define SB_EGL_BAD_CURRENT_SURFACE 0x3007
#define SB_EGL_BAD_DISPLAY 0x3008
#define SB_EGL_BAD_MATCH 0x3009
#define SB_EGL_BAD_NATIVE_PIXMAP 0x300A
#define SB_EGL_BAD_NATIVE_WINDOW 0x300B
#define SB_EGL_BAD_PARAMETER 0x300C
#define SB_EGL_BAD_SURFACE 0x300D
#define SB_EGL_BLUE_SIZE 0x3022
#define SB_EGL_BUFFER_SIZE 0x3020
#define SB_EGL_CONFIG_CAVEAT 0x3027
#define SB_EGL_CONFIG_ID 0x3028
#define SB_EGL_CORE_NATIVE_ENGINE 0x305B
#define SB_EGL_DEPTH_SIZE 0x3025
#define SB_EGL_DONT_CARE (SbEglInt32)(-1)
#define SB_EGL_DRAW 0x3059
#define SB_EGL_EXTENSIONS 0x3055
#define SB_EGL_FALSE 0
#define SB_EGL_GREEN_SIZE 0x3023
#define SB_EGL_HEIGHT 0x3056
#define SB_EGL_LARGEST_PBUFFER 0x3058
#define SB_EGL_LEVEL 0x3029
#define SB_EGL_MAX_PBUFFER_HEIGHT 0x302A
#define SB_EGL_MAX_PBUFFER_PIXELS 0x302B
#define SB_EGL_MAX_PBUFFER_WIDTH 0x302C
#define SB_EGL_NATIVE_RENDERABLE 0x302D
#define SB_EGL_NATIVE_VISUAL_ID 0x302E
#define SB_EGL_NATIVE_VISUAL_TYPE 0x302F
#define SB_EGL_NONE 0x3038
#define SB_EGL_NON_CONFORMANT_CONFIG 0x3051
#define SB_EGL_NOT_INITIALIZED 0x3001
#define SB_EGL_NO_CONTEXT (SbEglContext)(0)
#define SB_EGL_NO_DISPLAY (SbEglDisplay)(0)
#define SB_EGL_NO_SURFACE (SbEglSurface)(0)
#define SB_EGL_PBUFFER_BIT 0x0001
#define SB_EGL_PIXMAP_BIT 0x0002
#define SB_EGL_READ 0x305A
#define SB_EGL_RED_SIZE 0x3024
#define SB_EGL_SAMPLES 0x3031
#define SB_EGL_SAMPLE_BUFFERS 0x3032
#define SB_EGL_SLOW_CONFIG 0x3050
#define SB_EGL_STENCIL_SIZE 0x3026
#define SB_EGL_SUCCESS 0x3000
#define SB_EGL_SURFACE_TYPE 0x3033
#define SB_EGL_TRANSPARENT_BLUE_VALUE 0x3035
#define SB_EGL_TRANSPARENT_GREEN_VALUE 0x3036
#define SB_EGL_TRANSPARENT_RED_VALUE 0x3037
#define SB_EGL_TRANSPARENT_RGB 0x3052
#define SB_EGL_TRANSPARENT_TYPE 0x3034
#define SB_EGL_TRUE 1
#define SB_EGL_VENDOR 0x3053
#define SB_EGL_VERSION 0x3054
#define SB_EGL_WIDTH 0x3057
#define SB_EGL_WINDOW_BIT 0x0004

// EGL_VERSION_1_1
#define SB_EGL_BACK_BUFFER 0x3084
#define SB_EGL_BIND_TO_TEXTURE_RGB 0x3039
#define SB_EGL_BIND_TO_TEXTURE_RGBA 0x303A
#define SB_EGL_CONTEXT_LOST 0x300E
#define SB_EGL_MIN_SWAP_INTERVAL 0x303B
#define SB_EGL_MAX_SWAP_INTERVAL 0x303C
#define SB_EGL_MIPMAP_TEXTURE 0x3082
#define SB_EGL_MIPMAP_LEVEL 0x3083
#define SB_EGL_NO_TEXTURE 0x305C
#define SB_EGL_TEXTURE_2D 0x305F
#define SB_EGL_TEXTURE_FORMAT 0x3080
#define SB_EGL_TEXTURE_RGB 0x305D
#define SB_EGL_TEXTURE_RGBA 0x305E
#define SB_EGL_TEXTURE_TARGET 0x3081

// EGL_VERSION_1_2
#define SB_EGL_ALPHA_FORMAT 0x3088
#define SB_EGL_ALPHA_FORMAT_NONPRE 0x308B
#define SB_EGL_ALPHA_FORMAT_PRE 0x308C
#define SB_EGL_ALPHA_MASK_SIZE 0x303E
#define SB_EGL_BUFFER_PRESERVED 0x3094
#define SB_EGL_BUFFER_DESTROYED 0x3095
#define SB_EGL_CLIENT_APIS 0x308D
#define SB_EGL_COLORSPACE 0x3087
#define SB_EGL_COLORSPACE_sRGB 0x3089
#define SB_EGL_COLORSPACE_LINEAR 0x308A
#define SB_EGL_COLOR_BUFFER_TYPE 0x303F
#define SB_EGL_CONTEXT_CLIENT_TYPE 0x3097
#define SB_EGL_DISPLAY_SCALING 10000
#define SB_EGL_HORIZONTAL_RESOLUTION 0x3090
#define SB_EGL_LUMINANCE_BUFFER 0x308F
#define SB_EGL_LUMINANCE_SIZE 0x303D
#define SB_EGL_OPENGL_ES_BIT 0x0001
#define SB_EGL_OPENVG_BIT 0x0002
#define SB_EGL_OPENGL_ES_API 0x30A0
#define SB_EGL_OPENVG_API 0x30A1
#define SB_EGL_OPENVG_IMAGE 0x3096
#define SB_EGL_PIXEL_ASPECT_RATIO 0x3092
#define SB_EGL_RENDERABLE_TYPE 0x3040
#define SB_EGL_RENDER_BUFFER 0x3086
#define SB_EGL_RGB_BUFFER 0x308E
#define SB_EGL_SINGLE_BUFFER 0x3085
#define SB_EGL_SWAP_BEHAVIOR 0x3093
#define SB_EGL_UNKNOWN (SbEglInt) - 1
#define SB_EGL_VERTICAL_RESOLUTION 0x3091

// EGL_VERSION_1_3
#define SB_EGL_CONFORMANT 0x3042
#define SB_EGL_CONTEXT_CLIENT_VERSION 0x3098
#define SB_EGL_MATCH_NATIVE_PIXMAP 0x3041
#define SB_EGL_OPENGL_ES2_BIT 0x0004
#define SB_EGL_VG_ALPHA_FORMAT 0x3088
#define SB_EGL_VG_ALPHA_FORMAT_NONPRE 0x308B
#define SB_EGL_VG_ALPHA_FORMAT_PRE 0x308C
#define SB_EGL_VG_ALPHA_FORMAT_PRE_BIT 0x0040
#define SB_EGL_VG_COLORSPACE 0x3087
#define SB_EGL_VG_COLORSPACE_sRGB 0x3089
#define SB_EGL_VG_COLORSPACE_LINEAR 0x308A
#define SB_EGL_VG_COLORSPACE_LINEAR_BIT 0x0020

// EGL_VERSION_1_4
#define SB_EGL_DEFAULT_DISPLAY (SbEglNativeDisplayType)(0)
#define SB_EGL_MULTISAMPLE_RESOLVE_BOX_BIT 0x0200
#define SB_EGL_MULTISAMPLE_RESOLVE 0x3099
#define SB_EGL_MULTISAMPLE_RESOLVE_DEFAULT 0x309A
#define SB_EGL_MULTISAMPLE_RESOLVE_BOX 0x309B
#define SB_EGL_OPENGL_API 0x30A2
#define SB_EGL_OPENGL_BIT 0x0008
#define SB_EGL_SWAP_BEHAVIOR_PRESERVED_BIT 0x0400

// EGL_VERSION_1_5
#define SB_EGL_CONTEXT_MAJOR_VERSION 0x3098
#define SB_EGL_CONTEXT_MINOR_VERSION 0x30FB
#define SB_EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#define SB_EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY 0x31BD
#define SB_EGL_NO_RESET_NOTIFICATION 0x31BE
#define SB_EGL_LOSE_CONTEXT_ON_RESET 0x31BF
#define SB_EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001
#define SB_EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT 0x00000002
#define SB_EGL_CONTEXT_OPENGL_DEBUG 0x31B0
#define SB_EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE 0x31B1
#define SB_EGL_CONTEXT_OPENGL_ROBUST_ACCESS 0x31B2
#define SB_EGL_OPENGL_ES3_BIT 0x00000040
#define SB_EGL_CL_EVENT_HANDLE 0x309C
#define SB_EGL_SYNC_CL_EVENT 0x30FE
#define SB_EGL_SYNC_CL_EVENT_COMPLETE 0x30FF
#define SB_EGL_SYNC_PRIOR_COMMANDS_COMPLETE 0x30F0
#define SB_EGL_SYNC_TYPE 0x30F7
#define SB_EGL_SYNC_STATUS 0x30F1
#define SB_EGL_SYNC_CONDITION 0x30F8
#define SB_EGL_SIGNALED 0x30F2
#define SB_EGL_UNSIGNALED 0x30F3
#define SB_EGL_SYNC_FLUSH_COMMANDS_BIT 0x0001
#define SB_EGL_FOREVER 0xFFFFFFFFFFFFFFFFull
#define SB_EGL_TIMEOUT_EXPIRED 0x30F5
#define SB_EGL_CONDITION_SATISFIED 0x30F6
#define SB_EGL_NO_SYNC (SbEglSync)(0)
#define SB_EGL_SYNC_FENCE 0x30F9
#define SB_EGL_GL_COLORSPACE 0x309D
#define SB_EGL_GL_COLORSPACE_SRGB 0x3089
#define SB_EGL_GL_COLORSPACE_LINEAR 0x308A
#define SB_EGL_GL_RENDERBUFFER 0x30B9
#define SB_EGL_GL_TEXTURE_2D 0x30B1
#define SB_EGL_GL_TEXTURE_LEVEL 0x30BC
#define SB_EGL_GL_TEXTURE_3D 0x30B2
#define SB_EGL_GL_TEXTURE_ZOFFSET 0x30BD
#define SB_EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x30B3
#define SB_EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x30B4
#define SB_EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x30B5
#define SB_EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x30B6
#define SB_EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x30B7
#define SB_EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x30B8
#define SB_EGL_IMAGE_PRESERVED 0x30D2
#define SB_EGL_NO_IMAGE (SbEglImage)(0)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EGL_H_
