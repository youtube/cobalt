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
  void BeginTrace(const char* name) OVERRIDE {
    TRACE_EVENT_BEGIN0("glimp", name);
  }
  void EndTrace(const char* name) OVERRIDE { TRACE_EVENT_END0("glimp", name); }
};

GlimpToBaseTraceEventBridge s_glimp_to_base_trace_event_bridge;
#endif  // #if defined(ENABLE_GLIMP_TRACING)

GraphicsSystemEGL::GraphicsSystemEGL() {
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
#if !SB_HAS_QUIRK(NO_EGL_BIND_TO_TEXTURE)
    EGL_BIND_TO_TEXTURE_RGBA,
    EGL_TRUE,
#endif
    EGL_RENDERABLE_TYPE,
#if defined(GLES3_SUPPORTED)
    EGL_OPENGL_ES3_BIT,
#else
    EGL_OPENGL_ES2_BIT,
#endif  // #if defined(GLES3_SUPPORTED)
    EGL_NONE
  };

  EGLint num_configs;
  eglChooseConfig(display_, attribute_list, &config_, 1, &num_configs);

#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  // Try to allow preservation of the frame contents between swap calls --
  // this will allow rendering of only parts of the frame that have changed.
  DCHECK_EQ(EGL_SURFACE_TYPE, attribute_list[0]);
  EGLint& surface_type_value = attribute_list[1];

  if (eglGetError() != EGL_SUCCESS || num_configs == 0) {
    // Swap buffer preservation may not be supported. Try to find a config
    // without the feature.
    surface_type_value &= ~EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
    EGL_CALL(
        eglChooseConfig(display_, attribute_list, &config_, 1, &num_configs));
  }
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)

  DCHECK_EQ(1, num_configs);

#if defined(GLES3_SUPPORTED)
  resource_context_.emplace(display_, config_);
#endif
}

GraphicsSystemEGL::~GraphicsSystemEGL() {
  resource_context_ = base::nullopt;
  eglTerminate(display_);
}

scoped_ptr<Display> GraphicsSystemEGL::CreateDisplay(
    system_window::SystemWindow* system_window) {
  EGLNativeWindowType window_handle = GetHandleFromSystemWindow(system_window);
  return scoped_ptr<Display>(new DisplayEGL(display_, config_, window_handle));
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
  return scoped_ptr<TextureDataEGL>(
      new TextureDataPBO(&(resource_context_.value()), size, format));
#else
  return scoped_ptr<TextureDataEGL>(new TextureDataCPU(size, format));
#endif
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
