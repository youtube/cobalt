// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display.h"

#include "ui/gl/gl_bindings.h"

#import <Metal/Metal.h>

// From ANGLE's egl/eglext_angle.h.
#ifndef EGL_ANGLE_metal_shared_event_sync
#define EGL_ANGLE_metal_hared_event_sync 1
#define EGL_SYNC_METAL_SHARED_EVENT_ANGLE 0x34D8
#define EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE 0x34D9
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE 0x34DA
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE 0x34DB
#define EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE 0x34DC
#endif

namespace gl {

bool GLDisplayEGL::IsANGLEMetalSharedEventSyncSupported() {
  return this->ext->b_EGL_ANGLE_metal_shared_event_sync;
}

bool GLDisplayEGL::CreateMetalSharedEvent(
    metal::MTLSharedEventPtr* shared_event_out,
    uint64_t* signal_value_out) {
  DCHECK(g_driver_egl.fn.eglCreateSyncFn);
  DCHECK(g_driver_egl.fn.eglGetSyncAttribFn);
  if (!metal_shared_event_) {
    std::vector<EGLAttrib> attribs;
    attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE);
    attribs.push_back(0);
    attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE);
    attribs.push_back(0);
    attribs.push_back(EGL_NONE);
    EGLSync sync =
        eglCreateSync(display_, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, &attribs[0]);
    if (!sync)
      return false;
    // eglCopyMetalSharedEventANGLE retains the MTLSharedEvent object
    // once, so we can hold on to it for as long as we like, and must
    // release it upon destruction.
    metal_shared_event_ = static_cast<metal::MTLSharedEventPtr>(
        eglCopyMetalSharedEventANGLE(display_, sync));

    // The sync object is already enqueued for signaling in ANGLE's command
    // stream. Since the MTLSharedEvent is already retained, it's safe to delete
    // the sync object immediately.
    eglDestroySync(display_, sync);
  }

  // Create another sync object, perhaps redundantly the first time,
  // but with our specified signal value.
  ++metal_signaled_value_;
  std::vector<EGLAttrib> attribs;
  attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE);
  attribs.push_back(
      static_cast<EGLAttrib>(reinterpret_cast<uintptr_t>(metal_shared_event_)));
  attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE);
  attribs.push_back(metal_signaled_value_ & 0xFFFFFFFF);
  attribs.push_back(EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE);
  attribs.push_back((metal_signaled_value_ >> 32) & 0xFFFFFFFF);
  attribs.push_back(EGL_NONE);

  EGLSync sync =
      eglCreateSync(display_, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, &attribs[0]);
  if (!sync)
    return false;

  // The sync object is already enqueued for signaling in ANGLE's command
  // stream. Since the MTLSharedEvent is already retained, it's safe to delete
  // the sync object immediately.
  eglDestroySync(display_, sync);

  *shared_event_out = metal_shared_event_;
  *signal_value_out = metal_signaled_value_;
  return true;
}

void GLDisplayEGL::WaitForMetalSharedEvent(
    metal::MTLSharedEventPtr shared_event,
    uint64_t signal_value) {
  EGLAttrib attribs[] = {
      // Pass the Metal shared event as an EGLAttrib.
      EGL_SYNC_METAL_SHARED_EVENT_OBJECT_ANGLE,
      static_cast<EGLAttrib>(reinterpret_cast<uintptr_t>(shared_event)),
      // EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE is important as it requests
      // ANGLE to create an EGL sync object from the Metal shared event, but NOT
      // signal it to the specified value. The shared event is imported with
      // that signal value. The next call to eglWaitSync enqueue's a GPU wait to
      // wait for that value to be signaled by another command buffer.
      EGL_SYNC_CONDITION,
      EGL_SYNC_METAL_SHARED_EVENT_SIGNALED_ANGLE,
      // Encode the signaled value in two EGLAttribs.
      EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_LO_ANGLE,
      EGLAttrib(signal_value & 0xFFFFFFFF),
      EGL_SYNC_METAL_SHARED_EVENT_SIGNAL_VALUE_HI_ANGLE,
      EGLAttrib((signal_value >> 32) & 0xFFFFFFFF),
      EGL_NONE,
  };

  DCHECK(g_driver_egl.fn.eglCreateSyncFn);
  EGLSync sync =
      eglCreateSync(display_, EGL_SYNC_METAL_SHARED_EVENT_ANGLE, attribs);
  EGLBoolean res = eglWaitSync(display_, sync, 0);
  DCHECK(res == EGL_TRUE);
  // The wait on the sync object has been enqueued already, so it's safe to
  // destroy it now.
  eglDestroySync(display_, sync);
}

void GLDisplayEGL::CleanupMetalSharedEvent() {
  if (@available(macOS 10.14, *)) {
    [metal_shared_event_ release];
    metal_shared_event_ = nil;
  }
}

}  // namespace gl
