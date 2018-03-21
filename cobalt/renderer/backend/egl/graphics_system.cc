// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/backend/egl/graphics_system.h"

#include "base/debug/leak_annotations.h"
#if defined(ENABLE_GLIMP_TRACING)
#include "base/debug/trace_event.h"
#endif
#include "base/optional.h"
#if defined(GLES3_SUPPORTED)
#include "cobalt/renderer/backend/egl/texture_data_pbo.h"
#else
#include "cobalt/renderer/backend/egl/texture_data_cpu.h"
#endif

#include "cobalt/renderer/backend/egl/display.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#if defined(ENABLE_GLIMP_TRACING)
#include "glimp/tracing/tracing.h"
#endif

namespace cobalt {
namespace renderer {
namespace backend {

#if defined(ENABLE_GLIMP_TRACING)
// Hookup glimp tracing to Chromium's base trace_event tracing.
class GlimpToBaseTraceEventBridge : public glimp::TraceEventImpl {
 public:
  void BeginTrace(const char* name) override {
    TRACE_EVENT_BEGIN0("glimp", name);
  }
  void EndTrace(const char* name) override { TRACE_EVENT_END0("glimp", name); }
};

GlimpToBaseTraceEventBridge s_glimp_to_base_trace_event_bridge;
#endif  // #if defined(ENABLE_GLIMP_TRACING)

namespace {

struct ChooseConfigResult {
  ChooseConfigResult() : window_surface(EGL_NO_SURFACE) {}

  EGLConfig config;
  // This will be EGL_NO_SURFACE if no window was provided to find a config to
  // match with.
  EGLSurface window_surface;
};

// Returns a config that matches the |attribute_list|.  Optionally,
// |system_window| can also be provided in which case the config returned will
// also be guaranteed to match with it, and a EGLSurface will be returned
// for that window.
base::optional<ChooseConfigResult> ChooseConfig(
    EGLDisplay display, EGLint* attribute_list,
    system_window::SystemWindow* system_window) {
  if (!system_window) {
    // If there is no system window provided then this task is much easier,
    // just return the first config that matches the provided attributes.
    EGLint num_configs;
    EGLConfig config;
    eglChooseConfig(display, attribute_list, &config, 1, &num_configs);
    DCHECK_GE(1, num_configs);

    if (num_configs == 1) {
      ChooseConfigResult results;
      results.config = config;
      return results;
    } else {
      LOG(ERROR) << "Could not find a EGLConfig compatible with the specified "
                 << "attributes.";
      return base::nullopt;
    }
  }

  // Retrieve *all* configs that match the attribute list, and for each of them
  // test to see if we can successfully call eglCreateWindowSurface() with them.
  // Return the first config that succeeds the above test.
  EGLint num_configs = 0;
  eglChooseConfig(display, attribute_list, NULL, 0, &num_configs);
  CHECK_EQ(EGL_SUCCESS, eglGetError());
  CHECK_LT(0, num_configs);

  scoped_array<EGLConfig> configs(new EGLConfig[num_configs]);
  eglChooseConfig(display, attribute_list, configs.get(), num_configs,
                  &num_configs);

  EGLNativeWindowType native_window =
      (EGLNativeWindowType)(system_window->GetWindowHandle());

  for (EGLint i = 0; i < num_configs; ++i) {
    EGLSurface surface =
        eglCreateWindowSurface(display, configs[i], native_window, NULL);
    if (EGL_SUCCESS == eglGetError()) {
      // We did it, we found a config that allows us to successfully create
      // a surface.  Return the config along with the created surface.
      ChooseConfigResult result;
      result.config = configs[i];
      result.window_surface = surface;
      return result;
    }
  }

  // We could not find a config with the provided window, return a failure.
  LOG(ERROR) << "Could not find a EGLConfig compatible with the specified "
             << "EGLNativeWindowType object and attributes.";
  return base::nullopt;
}

}  // namespace

GraphicsSystemEGL::GraphicsSystemEGL(
    system_window::SystemWindow* system_window) {
#if defined(ENABLE_GLIMP_TRACING)
  // If glimp tracing is enabled, hook up glimp trace calls to Chromium's
  // base trace_event calls.
  glimp::SetTraceEventImplementation(&s_glimp_to_base_trace_event_bridge);
#endif  // #if defined(ENABLE_GLIMP_TRACING)

  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  CHECK_NE(EGL_NO_DISPLAY, display_);
  CHECK_EQ(EGL_SUCCESS, eglGetError());

  {
    // Despite eglTerminate() being used in the destructor, the current
    // mesa egl drivers still leak memory.
    ANNOTATE_SCOPED_MEMORY_LEAK;
    EGL_CALL(eglInitialize(display_, NULL, NULL));
  }

  // Setup our configuration to support RGBA and compatibility with PBuffer
  // objects (for offscreen rendering).
  EGLint attribute_list[] = {
    EGL_SURFACE_TYPE,  // this must be first
    EGL_WINDOW_BIT | EGL_PBUFFER_BIT
#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
        | EGL_SWAP_BEHAVIOR_PRESERVED_BIT
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
    ,
    EGL_RED_SIZE,
    8,
    EGL_GREEN_SIZE,
    8,
    EGL_BLUE_SIZE,
    8,
    EGL_ALPHA_SIZE,
    8,
    EGL_RENDERABLE_TYPE,
#if defined(GLES3_SUPPORTED)
    EGL_OPENGL_ES3_BIT,
#else
    EGL_OPENGL_ES2_BIT,
#endif  // #if defined(GLES3_SUPPORTED)
    EGL_NONE
  };

  base::optional<ChooseConfigResult> choose_config_results =
      ChooseConfig(display_, attribute_list, system_window);

#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  // Try to allow preservation of the frame contents between swap calls --
  // this will allow rendering of only parts of the frame that have changed.
  DCHECK_EQ(EGL_SURFACE_TYPE, attribute_list[0]);
  EGLint& surface_type_value = attribute_list[1];

  // Swap buffer preservation may not be supported. If we failed to find a
  // config with the EGL_SWAP_BEHAVIOR_PRESERVED_BIT attribute, try again
  // without it.
  if (!choose_config_results) {
    surface_type_value &= ~EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
    choose_config_results =
        ChooseConfig(display_, attribute_list, system_window);
  }
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)

  DCHECK(choose_config_results);

  config_ = choose_config_results->config;
  system_window_ = system_window;
  window_surface_ = choose_config_results->window_surface;

#if defined(GLES3_SUPPORTED)
  resource_context_.emplace(display_, config_);
#endif
}

GraphicsSystemEGL::~GraphicsSystemEGL() {
  resource_context_ = base::nullopt;

  if (window_surface_ != EGL_NO_SURFACE) {
    eglDestroySurface(display_, window_surface_);
  }

  eglTerminate(display_);
}

scoped_ptr<Display> GraphicsSystemEGL::CreateDisplay(
    system_window::SystemWindow* system_window) {
  EGLSurface surface;
  if (system_window == system_window_ && window_surface_ != EGL_NO_SURFACE) {
    // Hand-off our precreated window into the display being created.
    surface = window_surface_;
    window_surface_ = EGL_NO_SURFACE;
  } else {
    EGLNativeWindowType native_window =
        (EGLNativeWindowType)(system_window->GetWindowHandle());
    surface = eglCreateWindowSurface(display_, config_, native_window, NULL);
    CHECK_EQ(EGL_SUCCESS, eglGetError());
  }

  return scoped_ptr<Display>(new DisplayEGL(display_, surface));
}

scoped_ptr<GraphicsContext> GraphicsSystemEGL::CreateGraphicsContext() {
// If GLES3 is supported, we will make use of PBOs to allocate buffers for
// texture data and populate them on separate threads.  In order to access
// that data from graphics contexts created through this method, we must
// enable sharing between them and the resource context, which is why we
// must pass it in here.
#if defined(GLES3_SUPPORTED)
  ResourceContext* resource_context = &(resource_context_.value());
#else
  ResourceContext* resource_context = NULL;
#endif
  return scoped_ptr<GraphicsContext>(
      new GraphicsContextEGL(this, display_, config_, resource_context));
}

scoped_ptr<TextureDataEGL> GraphicsSystemEGL::AllocateTextureData(
    const math::Size& size, GLenum format) {
#if defined(GLES3_SUPPORTED)
  scoped_ptr<TextureDataEGL> texture_data(
      new TextureDataPBO(&(resource_context_.value()), size, format));
#else
  scoped_ptr<TextureDataEGL> texture_data(new TextureDataCPU(size, format));
#endif
  if (texture_data->CreationError()) {
    return scoped_ptr<TextureDataEGL>();
  } else {
    return texture_data.Pass();
  }
}

scoped_ptr<RawTextureMemoryEGL> GraphicsSystemEGL::AllocateRawTextureMemory(
    size_t size_in_bytes, size_t alignment) {
#if defined(GLES3_SUPPORTED)
  return scoped_ptr<RawTextureMemoryEGL>(new RawTextureMemoryPBO(
      &(resource_context_.value()), size_in_bytes, alignment));
#else
  return scoped_ptr<RawTextureMemoryEGL>(
      new RawTextureMemoryCPU(size_in_bytes, alignment));
#endif
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
