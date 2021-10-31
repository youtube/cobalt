// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_EGL_RESOURCE_CONTEXT_H_
#define COBALT_RENDERER_BACKEND_EGL_RESOURCE_CONTEXT_H_

#include <memory>
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace backend {

class TextureData;
class RawTextureMemory;

// The ResourceContext class encapsulates a GL context and a private thread
// upon which that GL context is always current.  The GL context is associated
// with a dummy surface.  The GL context will be shared with all other contexts
// created by the GraphicsSystem that owns this ResourceContext.  This class
// is useful when one wishes to create and allocate pixel buffer data (e.g. for
// a texture) from any thread, that can later be converted to a texture for
// a specific graphics context.  Resource management GL operations can be
// performed (from any thread) by calling the method
// RunSynchronouslyWithinResourceContext() and passing a function which one
// wishes to run under the resource management GL context.
class ResourceContext {
 public:
  ResourceContext(EGLDisplay display, EGLConfig config);
  ~ResourceContext();

  EGLContext context() const { return context_; }

  // This method can be used to execute a function on the resource context
  // thread, under which it can be assumed that the resource context is
  // current.
  void RunSynchronouslyWithinResourceContext(const base::Closure& function);

  // When called, asserts that the current thread is the resource context
  // thread, which is the only thread which has the resource context current.
  void AssertWithinResourceContext();

 private:
  // Called once upon construction on the resource context thread.
  void MakeCurrent();

  // Releases the current context from the resource context thread, and then
  // destroys the resource context (and dummy surface).  This is intended to
  // be called from the destructor.
  void ShutdownOnResourceThread();

  EGLDisplay display_;

  // The EGL/OpenGL ES context hosted by this GraphicsContextEGL object.
  EGLContext context_;
  EGLConfig config_;

  // Since this context will be used only for resource management, it will
  // not need an output surface associated with it.  We create a dummy surface
  // to satisfy EGL requirements.
  EGLSurface null_surface_;

  // A private thread on which all resource management GL operations can be
  // performed.  The resource context will always be current on this thread.
  base::Thread thread_;

  friend class ScopedMakeCurrent;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_RESOURCE_CONTEXT_H_
