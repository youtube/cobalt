// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_MAKE_CURRENT_H_
#define UI_GL_SCOPED_MAKE_CURRENT_H_

#include "base/memory/scoped_refptr.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLContext;
class GLSurface;
}

namespace ui {

// ScopedMakeCurrent makes given context current with the given surface. If this
// fails, Succeeded() returns false. It also restores the previous context on
// destruction and asserts that this succeeds. Users can optionally use
// Restore() to check if restoring the previous context succeeds. This prevents
// the assert during destruction.
class GL_EXPORT ScopedMakeCurrent {
 public:
  ScopedMakeCurrent(gl::GLContext* context, gl::GLSurface* surface);

  ScopedMakeCurrent(const ScopedMakeCurrent&) = delete;
  ScopedMakeCurrent& operator=(const ScopedMakeCurrent&) = delete;

  ~ScopedMakeCurrent();

  // Returns whether the |context_| is current.
  bool IsContextCurrent() { return is_context_current_; }

 private:
  scoped_refptr<gl::GLContext> previous_context_;
  scoped_refptr<gl::GLSurface> previous_surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  bool is_context_current_ = false;
};

}  // namespace ui

#endif  // UI_GL_SCOPED_MAKE_CURRENT_H_
