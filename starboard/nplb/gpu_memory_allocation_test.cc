// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <unistd.h>

#include "starboard/egl.h"
#include "starboard/gles.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EGL_CALL(interface, x)                                   \
  do {                                                           \
    (interface)->x;                                              \
    EXPECT_EQ((interface)->eglGetError(), SB_EGL_SUCCESS) << #x; \
  } while (false)

#define GL_CALL(interface, x)                                   \
  do {                                                          \
    (interface)->x;                                             \
    EXPECT_EQ((interface)->glGetError(), SB_GL_NO_ERROR) << #x; \
  } while (false)

namespace nplb {
namespace {

const SbEglInt32 kAttributeList[] = {SB_EGL_RED_SIZE,
                                     8,
                                     SB_EGL_GREEN_SIZE,
                                     8,
                                     SB_EGL_BLUE_SIZE,
                                     8,
                                     SB_EGL_ALPHA_SIZE,
                                     8,
                                     SB_EGL_SURFACE_TYPE,
                                     SB_EGL_WINDOW_BIT | SB_EGL_PBUFFER_BIT,
                                     SB_EGL_RENDERABLE_TYPE,
                                     SB_EGL_OPENGL_ES2_BIT,
                                     SB_EGL_NONE};

const SbEglInt32 kContextAttribList[] = {SB_EGL_CONTEXT_CLIENT_VERSION, 2,
                                         SB_EGL_NONE};

TEST(GpuMemoryAllocationTest, Allocate300MB) {
  const SbEglInterface* egl = SbGetEglInterface();
  const SbGlesInterface* gles = SbGetGlesInterface();

  if (!egl || !gles) {
    return;
  }

  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  SbWindow window = SbWindowCreate(&options);
  ASSERT_TRUE(SbWindowIsValid(window));

  SbEglDisplay display = egl->eglGetDisplay(SB_EGL_DEFAULT_DISPLAY);
  ASSERT_NE(SB_EGL_NO_DISPLAY, display);

  EGL_CALL(egl, eglInitialize(display, nullptr, nullptr));

  SbEglInt32 num_configs = 0;
  EGL_CALL(egl,
           eglChooseConfig(display, kAttributeList, nullptr, 0, &num_configs));
  ASSERT_GT(num_configs, 0);

  SbEglConfig* configs = new SbEglConfig[num_configs];
  EGL_CALL(egl, eglChooseConfig(display, kAttributeList, configs, num_configs,
                                &num_configs));

  SbEglNativeWindowType native_window =
      (SbEglNativeWindowType)SbWindowGetPlatformHandle(window);
  SbEglSurface surface = SB_EGL_NO_SURFACE;
  SbEglConfig config = configs[0];

  for (int i = 0; i < num_configs; ++i) {
    surface = egl->eglCreateWindowSurface(display, configs[i], native_window,
                                          nullptr);
    if (egl->eglGetError() == SB_EGL_SUCCESS) {
      config = configs[i];
      break;
    }
  }
  ASSERT_NE(SB_EGL_NO_SURFACE, surface);
  delete[] configs;

  SbEglContext context = egl->eglCreateContext(
      display, config, SB_EGL_NO_CONTEXT, kContextAttribList);
  ASSERT_NE(SB_EGL_NO_CONTEXT, context);

  EGL_CALL(egl, eglMakeCurrent(display, surface, surface, context));

  // Allocate 300MB of GPU memory using a vertex buffer
  SbGlUInt32 buffer = 0;
  GL_CALL(gles, glGenBuffers(1, &buffer));
  GL_CALL(gles, glBindBuffer(SB_GL_ARRAY_BUFFER, buffer));
  usleep(60ULL * 1000 * 1000);

  // Allocate 200MB of CPU memory
  const size_t kCpuAllocationSize = 200 * 1024 * 1024;
  void* cpu_memory = malloc(kCpuAllocationSize);
  ASSERT_NE(nullptr, cpu_memory);
  memset(cpu_memory, 0xAA, kCpuAllocationSize);
  SbLogRaw(
      "Allocated 200MB CPU memory. Sleeping for 1 minutes for "
      "verification...\n");
  usleep(60ULL * 1000 * 1000);

  const size_t kAllocationSize = 300 * 1024 * 1024;
  gles->glBufferData(SB_GL_ARRAY_BUFFER, kAllocationSize, nullptr,
                     SB_GL_STATIC_DRAW);
  EXPECT_EQ(gles->glGetError(), SB_GL_NO_ERROR);
  SbLogRaw(
      "Allocated 300MB GPU memory. Sleeping for 3 minutes for "
      "verification...\n");
  usleep(180ULL * 1000 * 1000);

  // Clean up
  free(cpu_memory);
  GL_CALL(gles, glDeleteBuffers(1, &buffer));
  EGL_CALL(egl, eglMakeCurrent(display, SB_EGL_NO_SURFACE, SB_EGL_NO_SURFACE,
                               SB_EGL_NO_CONTEXT));
  EGL_CALL(egl, eglDestroyContext(display, context));
  EGL_CALL(egl, eglDestroySurface(display, surface));
  EGL_CALL(egl, eglTerminate(display));
  EXPECT_TRUE(SbWindowDestroy(window));
}

}  // namespace
}  // namespace nplb
