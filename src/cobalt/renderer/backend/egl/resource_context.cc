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

#include "cobalt/renderer/backend/egl/resource_context.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

ResourceContext::ResourceContext(EGLDisplay display, EGLConfig config)
    : display_(display), config_(config), thread_("GL Texture Resource") {
  context_ = CreateGLES3Context(display, config, EGL_NO_CONTEXT);

  // Create a dummy EGLSurface object to be assigned as the target surface
  // when we need to make OpenGL calls that do not depend on a surface (e.g.
  // creating a texture).
  EGLint null_surface_attrib_list[] = {
      EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE,
  };
  null_surface_ =
      eglCreatePbufferSurface(display, config, null_surface_attrib_list);
  CHECK_EQ(EGL_SUCCESS, eglGetError());

  // Start the resource context thread and immediately make the resource context
  // current on that thread, so that subsequent calls to
  // RunSynchronouslyWithinResourceContext() can assume it is current already.
  thread_.Start();
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ResourceContext::MakeCurrent, base::Unretained(this)));
}

ResourceContext::~ResourceContext() {
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ResourceContext::ShutdownOnResourceThread, base::Unretained(this)));
}

namespace {
void RunAndSignal(const base::Closure& function, base::WaitableEvent* event) {
  function.Run();
  event->Signal();
}
}

void ResourceContext::RunSynchronouslyWithinResourceContext(
    const base::Closure& function) {
  DCHECK_NE(thread_.message_loop(), MessageLoop::current()) <<
      "This method should not be called within the resource context thread.";

  base::WaitableEvent event(true, false);
  thread_.message_loop()->PostTask(FROM_HERE,
                                   base::Bind(&RunAndSignal, function, &event));
  event.Wait();
}

void ResourceContext::AssertWithinResourceContext() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
}

void ResourceContext::MakeCurrent() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  EGL_CALL(eglMakeCurrent(display_, null_surface_, null_surface_, context_));
}

void ResourceContext::ShutdownOnResourceThread() {
  EGL_CALL(eglMakeCurrent(
      display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

  EGL_CALL(eglDestroySurface(display_, null_surface_));
  EGL_CALL(eglDestroyContext(display_, context_));
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
