// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FILTERS_CONTEXT_3D_H_
#define COBALT_MEDIA_FILTERS_CONTEXT_3D_H_

class GrContext;

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cobalt {
namespace media {

// This struct can be used to make media use gpu::gles2::GLES2Interface and
// GrContext.
// Usage:
// gpu::gles2::GLES2Interface* gl = ...;
// GrContext* gr_context = ...;
// Context3D context_3d(gl, gr_context);

struct Context3D {
  Context3D() : gl(NULL), gr_context(NULL) {}
  Context3D(gpu::gles2::GLES2Interface* gl_, class GrContext* gr_context_)
      : gl(gl_), gr_context(gr_context_) {}

  gpu::gles2::GLES2Interface* gl;
  class GrContext* gr_context;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FILTERS_CONTEXT_3D_H_
