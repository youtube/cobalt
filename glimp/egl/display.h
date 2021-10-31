/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLIMP_EGL_DISPLAY_H_
#define GLIMP_EGL_DISPLAY_H_

#include <set>

#include "glimp/egl/config.h"
#include "glimp/egl/display_impl.h"
#include "glimp/gles/context.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace egl {

// Encapsulates the concept of an EGL display.  There is usually only one of
// these per process, and it represents the entire graphics system.  It is
// the highest level object, and the "factory" responsible for generating
// graphics contexts.  It is a platform-independent object that wraps a
// platform-dependent DisplayImpl object which must be injected into the Display
// upon construction.
class Display {
 public:
  // In order to create a display, it must have a platform-specific
  // implementation injected into it, where many methods will forward to.
  explicit Display(nb::scoped_ptr<DisplayImpl> display_impl);
  ~Display();

  void GetVersionInfo(EGLint* major, EGLint* minor);

  bool ChooseConfig(const EGLint* attrib_list,
                    EGLConfig* configs,
                    EGLint config_size,
                    EGLint* num_config);
  bool ConfigIsValid(EGLConfig config);

  EGLSurface CreateWindowSurface(EGLConfig config,
                                 EGLNativeWindowType win,
                                 const EGLint* attrib_list);
  EGLSurface CreatePbufferSurface(EGLConfig config, const EGLint* attrib_list);

  bool SurfaceIsValid(EGLSurface surface);
  bool DestroySurface(EGLSurface surface);

  EGLContext CreateContext(EGLConfig config,
                           EGLContext share_context,
                           const EGLint* attrib_list);
  bool ContextIsValid(EGLContext context);
  bool DestroyContext(EGLContext ctx);

  bool MakeCurrent(EGLSurface draw, EGLSurface read, EGLContext ctx);
  bool SwapBuffers(EGLSurface surface);
  bool SwapInterval(EGLint interval);

  DisplayImpl* impl() const { return impl_.get(); }

 private:
  nb::scoped_ptr<DisplayImpl> impl_;

  // Keeps track of all created but not destroyed surfaces.
  std::set<Surface*> active_surfaces_;

  // Keeps track of all created but not destroyed contexts.
  std::set<gles::Context*> active_contexts_;
};

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_DISPLAY_H_
