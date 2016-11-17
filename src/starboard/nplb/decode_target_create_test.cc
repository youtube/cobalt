// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

// This must come after gtest, because it includes GL, which can include X11,
// which will define None to be 0L, which conflicts with gtest.
#include "starboard/decode_target.h"  // NOLINT(build/include_order)

#if SB_VERSION(3) && SB_HAS(GRAPHICS)

#if SB_HAS(BLITTER)
#include "starboard/blitter.h"
#include "starboard/nplb/blitter_helpers.h"
#endif

namespace starboard {
namespace nplb {
namespace {

#if SB_HAS(BLITTER)
const int kWidth = 128;
const int kHeight = 128;

TEST(SbDecodeTargetTest, SunnyDayCreate) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  SbDecodeTarget target =
      SbDecodeTargetCreate(kSbDecodeTargetFormat1PlaneRGBA, &surface);
  EXPECT_TRUE(SbDecodeTargetIsValid(target));
  if (SbDecodeTargetIsValid(target)) {
    SbBlitterSurface plane =
        SbDecodeTargetGetPlane(target, kSbDecodeTargetPlaneRGBA);
    EXPECT_TRUE(SbBlitterIsSurfaceValid(plane));
  }
  SbDecodeTargetDestroy(target);
  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}
#else  // SB_HAS(BLITTER)
// clang-format off
EGLint const kAttributeList[] = {
  EGL_RED_SIZE, 8,
  EGL_GREEN_SIZE, 8,
  EGL_BLUE_SIZE, 8,
  EGL_ALPHA_SIZE, 8,
  EGL_STENCIL_SIZE, 0,
  EGL_BUFFER_SIZE, 32,
  EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
  EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
  EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
  EGL_NONE
};
// clang-format on

class SbDecodeTargetTest : public testing::Test {
 protected:
  void SetUp() SB_OVERRIDE {
    SbWindowOptions options;
    SbWindowSetDefaultOptions(&options);
    window_ = SbWindowCreate(&options);

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(EGL_NO_DISPLAY, display_);

    eglInitialize(display_, NULL, NULL);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    EGLint num_configs = 0;
    eglChooseConfig(display_, kAttributeList, NULL, 0, &num_configs);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(0, num_configs);

    // Allocate space to receive the matching configs and retrieve them.
    EGLConfig* configs = new EGLConfig[num_configs];
    eglChooseConfig(display_, kAttributeList, configs, num_configs,
                    &num_configs);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    EGLNativeWindowType native_window =
        (EGLNativeWindowType)SbWindowGetPlatformHandle(window_);
    EGLConfig config;

    // Find the first config that successfully allow a window surface to be
    // created.
    surface_ = EGL_NO_SURFACE;
    for (int config_number = 0; config_number < num_configs; ++config_number) {
      config = configs[config_number];
      surface_ = eglCreateWindowSurface(display_, config, native_window, NULL);
      if (EGL_SUCCESS == eglGetError())
        break;
    }
    ASSERT_NE(EGL_NO_SURFACE, surface_);

    delete[] configs;

    // Create the GLES2 or GLEX3 Context.
    context_ = EGL_NO_CONTEXT;
    EGLint context_attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE,
    };
#if defined(GLES3_SUPPORTED)
    // Attempt to create an OpenGL ES 3.0 context.
    context_ =
        eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attrib_list);
#endif
    if (context_ == EGL_NO_CONTEXT) {
      // Create an OpenGL ES 2.0 context.
      context_attrib_list[1] = 2;
      context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT,
                                  context_attrib_list);
    }
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(EGL_NO_CONTEXT, context_);

    // connect the context to the surface
    eglMakeCurrent(display_, surface_, surface_, context_);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
  }

  void TearDown() SB_OVERRIDE {
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    EXPECT_EQ(EGL_SUCCESS, eglGetError());
    eglDestroyContext(display_, context_);
    EXPECT_EQ(EGL_SUCCESS, eglGetError());
    eglDestroySurface(display_, surface_);
    EXPECT_EQ(EGL_SUCCESS, eglGetError());
    eglTerminate(display_);
    EXPECT_EQ(EGL_SUCCESS, eglGetError());
    SbWindowDestroy(window_);
  }

  EGLContext context_;
  EGLDisplay display_;
  EGLSurface surface_;
  SbWindow window_;
};

TEST_F(SbDecodeTargetTest, SunnyDayCreate) {
  // Generate a texture to put in the SbDecodeTarget.
  GLuint texture_handle;
  glGenTextures(1, &texture_handle);
  glBindTexture(GL_TEXTURE_2D, texture_handle);
  glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA, 256, 256, 0 /*border*/,
               GL_RGBA8, GL_UNSIGNED_BYTE, NULL /*data*/);
  glBindTexture(GL_TEXTURE_2D, 0);

  SbDecodeTarget target = SbDecodeTargetCreate(
      display_, context_, kSbDecodeTargetFormat1PlaneRGBA, &texture_handle);
  GLuint plane = SbDecodeTargetGetPlane(target, kSbDecodeTargetPlaneRGBA);
  EXPECT_EQ(texture_handle, plane);
  SbDecodeTargetDestroy(target);
  glDeleteTextures(1, &texture_handle);
}

#endif  // SB_HAS(BLITTER)

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_VERSION(3) && SB_HAS(GRAPHICS)
