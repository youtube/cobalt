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

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#include <iomanip>

#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"
#include "starboard/window.h"

namespace {

EGLint const kAttributeList[] = {EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_ALPHA_SIZE,
                                 8,
                                 EGL_STENCIL_SIZE,
                                 0,
                                 EGL_BUFFER_SIZE,
                                 32,
                                 EGL_SURFACE_TYPE,
                                 EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                 EGL_COLOR_BUFFER_TYPE,
                                 EGL_RGB_BUFFER,
                                 EGL_CONFORMANT,
                                 EGL_OPENGL_ES2_BIT,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_ES2_BIT,
                                 EGL_NONE};
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

  EGLDisplay display_;
  EGLSurface surface_;
  EGLContext context_;

  EGLint egl_surface_width_;
  EGLint egl_surface_height_;
};

Application::Application() {
  frame_ = 0;

  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  window_ = SbWindowCreate(&options);
  SB_CHECK(SbWindowIsValid(window_));

  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  SB_CHECK(EGL_NO_DISPLAY != display_);

  eglInitialize(display_, NULL, NULL);
  SB_CHECK(EGL_SUCCESS == eglGetError());

  // Some EGL drivers can return a first config that doesn't allow
  // eglCreateWindowSurface(), with no differences in EGLConfig attribute values
  // from configs that do allow that. To handle that, we have to attempt
  // eglCreateWindowSurface() until we find a config that succeeds.

  // First, query how many configs match the given attribute list.
  EGLint num_configs = 0;
  eglChooseConfig(display_, kAttributeList, NULL, 0, &num_configs);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  SB_CHECK(0 != num_configs);

  // Allocate space to receive the matching configs and retrieve them.
  EGLConfig* configs = reinterpret_cast<EGLConfig*>(
      SbMemoryAllocate(num_configs * sizeof(EGLConfig)));
  eglChooseConfig(display_, kAttributeList, configs, num_configs, &num_configs);
  SB_CHECK(EGL_SUCCESS == eglGetError());

  EGLNativeWindowType native_window =
      (EGLNativeWindowType)SbWindowGetPlatformHandle(window_);
  EGLConfig config;

  // Find the first config that successfully allow a window surface to be
  // created.
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface_ = eglCreateWindowSurface(display_, config, native_window, NULL);
    if (EGL_SUCCESS == eglGetError())
      break;
  }
  SB_DCHECK(surface_ != EGL_NO_SURFACE);

  SbMemoryFree(configs);

  eglQuerySurface(display_, surface_, EGL_WIDTH, &egl_surface_width_);
  eglQuerySurface(display_, surface_, EGL_HEIGHT, &egl_surface_height_);
  SB_DCHECK(egl_surface_width_ > 0);
  SB_DCHECK(egl_surface_height_ > 0);

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
    context_ =
        eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attrib_list);
  }
  SB_CHECK(EGL_SUCCESS == eglGetError());
  SB_CHECK(context_ != EGL_NO_CONTEXT);

  /* connect the context to the surface */
  eglMakeCurrent(display_, surface_, surface_, context_);
  SB_CHECK(EGL_SUCCESS == eglGetError());

  RenderScene();
}

Application::~Application() {
  // Cleanup all used resources.
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  eglDestroyContext(display_, context_);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  eglDestroySurface(display_, surface_);
  SB_CHECK(EGL_SUCCESS == eglGetError());
  eglTerminate(display_);
  SB_CHECK(EGL_SUCCESS == eglGetError());
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
  glEnable(GL_SCISSOR_TEST);

  float radian = 2 * M_PI * frame_ / 600.0f;
  int offset_x = 300 * sin(radian);
  int offset_y = 300 * cos(radian);

  int block_width = egl_surface_width_ / 16;
  int block_height = egl_surface_height_ / 9;

  int center_x = (egl_surface_width_ - block_width) / 2;
  int center_y = (egl_surface_height_ - block_height) / 2;

  glScissor(center_x + offset_x, center_y + offset_y, block_width,
            block_height);
  SB_CHECK(GL_NO_ERROR == glGetError());

  glClearColor(getIntensity(frame_, 55.0f), getIntensity(frame_, 60.0f),
               getIntensity(frame_, 62.5f), 1.0);
  SB_CHECK(GL_NO_ERROR == glGetError());

  glClear(GL_COLOR_BUFFER_BIT);
  SB_CHECK(EGL_SUCCESS == eglGetError());

  glFlush();
  SB_CHECK(GL_NO_ERROR == glGetError());

  eglSwapBuffers(display_, surface_);
  SB_CHECK(EGL_SUCCESS == eglGetError());

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
