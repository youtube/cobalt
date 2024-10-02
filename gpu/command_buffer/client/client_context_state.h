// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ContextState class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_CONTEXT_STATE_H_

#include <GLES3/gl3.h>
#include "gles2_impl_export.h"

namespace gpu {
namespace gles2 {

struct GLES2_IMPL_EXPORT ClientContextState {
  ClientContextState();
  ~ClientContextState();

  // Returns true if state was cached in which case 'enabled' will be set to the
  // current state.
  bool GetEnabled(GLenum cap, bool* enabled) const;

  // Sets the state of a capability.
  // Returns true if the capability is one that is cached.
  // 'changed' will be true if the state was different from 'enabled.
  bool SetCapabilityState(GLenum cap, bool enabled, bool* changed);

  // Cache the user's valid viewport.
  void SetViewport(GLint x, GLint y, GLsizei width, GLsizei height);

  #include "gpu/command_buffer/client/client_context_state_autogen.h"

  EnableFlags enable_flags;

  GLint viewport_x = 0;
  GLint viewport_y = 0;
  GLsizei viewport_width = 0;
  GLsizei viewport_height = 0;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_CONTEXT_STATE_H_
