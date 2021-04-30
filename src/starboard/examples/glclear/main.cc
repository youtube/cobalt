// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <math.h>

#include <iomanip>

#include "starboard/common/log.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/memory.h"
#include "starboard/system.h"
#include "starboard/window.h"

#include "starboard/egl.h"
#include "starboard/gles.h"

#define EGL_CALL(x)                                                    \
  do {                                                                 \
    SbGetEglInterface()->x;                                            \
    SB_DCHECK((SbGetEglInterface()->eglGetError()) == SB_EGL_SUCCESS); \
  } while (false)

#define EGL_CALL_SIMPLE(x) (SbGetEglInterface()->x)

#define GL_CALL(x)                                                     \
  do {                                                                 \
    SbGetGlesInterface()->x;                                           \
    SB_DCHECK((SbGetGlesInterface()->glGetError()) == SB_GL_NO_ERROR); \
  } while (false)

#define GL_CALL_SIMPLE(x) (SbGetGlesInterface()->x)

namespace {

SbEglInt32 const kAttributeList[] = {SB_EGL_RED_SIZE,
                                     8,
                                     SB_EGL_GREEN_SIZE,
                                     8,
                                     SB_EGL_BLUE_SIZE,
                                     8,
                                     SB_EGL_ALPHA_SIZE,
                                     8,
                                     SB_EGL_STENCIL_SIZE,
                                     0,
                                     SB_EGL_BUFFER_SIZE,
                                     32,
                                     SB_EGL_SURFACE_TYPE,
                                     SB_EGL_WINDOW_BIT | SB_EGL_PBUFFER_BIT,
                                     SB_EGL_COLOR_BUFFER_TYPE,
                                     SB_EGL_RGB_BUFFER,
                                     SB_EGL_CONFORMANT,
                                     SB_EGL_OPENGL_ES2_BIT,
                                     SB_EGL_RENDERABLE_TYPE,
                                     SB_EGL_OPENGL_ES2_BIT,
                                     SB_EGL_NONE};
}  // namespace

class Application {
 public:
  Application();
  ~Application();

 private:
  // The callback function passed to SbEventSchedule().  Its purpose is to
  // forward to the non-static RenderScene() method.
  static void RenderSceneEventCallback(void* param);

  // Renders one frame of the animated scene, incrementing |frame_| each time
  // it is called.
  void RenderScene();

  // The current frame we are rendering, initialized to 0 and incremented after
  // each frame.
  int frame_;

  // The SbWindow within which we will perform our rendering.
  SbWindow window_;

  SbEglDisplay display_;
  SbEglSurface surface_;
  SbEglContext context_;

  SbEglInt32 egl_surface_width_;
  SbEglInt32 egl_surface_height_;
};

Application::Application() {
  frame_ = 0;

  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  window_ = SbWindowCreate(&options);
  SB_CHECK(SbWindowIsValid(window_));

  display_ = EGL_CALL_SIMPLE(eglGetDisplay(SB_EGL_DEFAULT_DISPLAY));
  SB_CHECK(SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));
  SB_CHECK(SB_EGL_NO_DISPLAY != display_);

  EGL_CALL(eglInitialize(display_, NULL, NULL));

  // Some EGL drivers can return a first config that doesn't allow
  // eglCreateWindowSurface(), with no differences in SbEglConfig attribute
  // values from configs that do allow that. To handle that, we have to attempt
  // eglCreateWindowSurface() until we find a config that succeeds.

  // First, query how many configs match the given attribute list.
  SbEglInt32 num_configs = 0;
  EGL_CALL(eglChooseConfig(display_, kAttributeList, NULL, 0, &num_configs));
  SB_CHECK(0 != num_configs);

  // Allocate space to receive the matching configs and retrieve them.
  SbEglConfig* configs = reinterpret_cast<SbEglConfig*>(
      SbMemoryAllocate(num_configs * sizeof(SbEglConfig)));
  EGL_CALL(eglChooseConfig(display_, kAttributeList, configs, num_configs,
                           &num_configs));

  SbEglNativeWindowType native_window =
      (SbEglNativeWindowType)SbWindowGetPlatformHandle(window_);
  SbEglConfig config;

  // Find the first config that successfully allow a window surface to be
  // created.
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface_ = EGL_CALL_SIMPLE(
        eglCreateWindowSurface(display_, config, native_window, NULL));
    if (SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()))
      break;
  }
  SB_DCHECK(surface_ != SB_EGL_NO_SURFACE);

  SbMemoryDeallocate(configs);

  EGL_CALL(
      eglQuerySurface(display_, surface_, SB_EGL_WIDTH, &egl_surface_width_));
  EGL_CALL(
      eglQuerySurface(display_, surface_, SB_EGL_HEIGHT, &egl_surface_height_));
  SB_DCHECK(egl_surface_width_ > 0);
  SB_DCHECK(egl_surface_height_ > 0);

  // Create the GLES2 or GLEX3 Context.
  context_ = SB_EGL_NO_CONTEXT;
  SbEglInt32 context_attrib_list[] = {
      SB_EGL_CONTEXT_CLIENT_VERSION, 3, SB_EGL_NONE,
  };
#if SB_API_VERSION < 12 && defined(GLES3_SUPPORTED)
  // Attempt to create an OpenGL ES 3.0 context.
  context_ = EGL_CALL_SIMPLE(eglCreateContext(
      display_, config, SB_EGL_NO_CONTEXT, context_attrib_list));
#endif
  if (context_ == SB_EGL_NO_CONTEXT) {
    // Create an OpenGL ES 2.0 context.
    context_attrib_list[1] = 2;
    context_ = EGL_CALL_SIMPLE(eglCreateContext(
        display_, config, SB_EGL_NO_CONTEXT, context_attrib_list));
  }
  SB_CHECK(SB_EGL_SUCCESS == EGL_CALL_SIMPLE(eglGetError()));
  SB_CHECK(context_ != SB_EGL_NO_CONTEXT);

  /* connect the context to the surface */
  EGL_CALL(eglMakeCurrent(display_, surface_, surface_, context_));

  RenderScene();
}

Application::~Application() {
  // Cleanup all used resources.
  EGL_CALL(eglMakeCurrent(display_, SB_EGL_NO_SURFACE, SB_EGL_NO_SURFACE,
                          SB_EGL_NO_CONTEXT));
  EGL_CALL(eglDestroyContext(display_, context_));
  EGL_CALL(eglDestroySurface(display_, surface_));
  EGL_CALL(eglTerminate(display_));
  SbWindowDestroy(window_);
}

void Application::RenderSceneEventCallback(void* param) {
  // Forward the call to the application instance specified as the parameter.
  Application* application = static_cast<Application*>(param);
  application->RenderScene();
}

namespace {

float getIntensity(int frame, float rate) {
  float radian = 2 * M_PI * frame / rate;
  return 0.5 + 0.5 * sin(radian);
}

}  // namespace

void Application::RenderScene() {
  // Render a moving and color changing rectangle using glClear() and
  // glScissor().
  GL_CALL_SIMPLE(glEnable(SB_GL_SCISSOR_TEST));

  float radian = 2 * M_PI * frame_ / 600.0f;
  int offset_x = egl_surface_height_ * sin(radian) / 3.6;
  int offset_y = egl_surface_height_ * cos(radian) / 3.6;

  int block_width = egl_surface_width_ / 16;
  int block_height = egl_surface_height_ / 9;

  int center_x = (egl_surface_width_ - block_width) / 2;
  int center_y = (egl_surface_height_ - block_height) / 2;

  GL_CALL(glScissor(center_x + offset_x, center_y + offset_y, block_width,
                    block_height));
  GL_CALL(glClearColor(getIntensity(frame_, 55.0f), getIntensity(frame_, 60.0f),
                       getIntensity(frame_, 62.5f), 1.0));
  GL_CALL(glClear(SB_GL_COLOR_BUFFER_BIT));
  GL_CALL(glFlush());
  EGL_CALL(eglSwapBuffers(display_, surface_));

  // Schedule another frame render ASAP.
  SbEventSchedule(&Application::RenderSceneEventCallback, this, 0);
  ++frame_;
}

Application* s_application = NULL;

// Simple Starboard window event handling to kick off our color animating
// application.
void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      // Create the application, after which it will use SbEventSchedule()
      // on itself to trigger a frame update until the application is
      // terminated.
      s_application = new Application();
    } break;

    case kSbEventTypeStop: {
      // Shutdown the application.
      delete s_application;
    } break;

    default: {}
  }
}
