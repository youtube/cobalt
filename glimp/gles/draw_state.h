/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_DRAW_STATE_H_
#define GLIMP_GLES_DRAW_STATE_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "glimp/egl/surface.h"
#include "glimp/gles/blend_state.h"
#include "glimp/gles/buffer.h"
#include "glimp/gles/cull_face_state.h"
#include "glimp/gles/framebuffer.h"
#include "glimp/gles/program.h"
#include "glimp/gles/sampler.h"
#include "glimp/gles/vertex_attribute.h"
#include "nb/rect.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

// Types passed in as parameters to draw calls (like DrawArrays()) to
// describe the set of only enabled vertex attributes.
typedef std::vector<std::pair<unsigned int, VertexAttributeArray*> >
    EnabledVertexAttributeList;

// If a vertex attribute constant is specified (e.g. through a call to
// glVertexAttribXfv()) for a location, and the vertex attribute array is
// disabled at that location, then this constant value will be included into
// the draw state.
typedef std::vector<std::pair<unsigned int, VertexAttributeConstant*> >
    ConstantVertexAttributeList;

// Similar to EnabledVertexAttributeList, but lists only texture units with
// textures bound to them.
typedef std::vector<std::pair<unsigned int, Texture*> > EnabledTextureList;

struct ViewportState {
  ViewportState() : rect(-1, -1, -1, -1) {}

  nb::Rect<int> rect;
};

struct ScissorState {
  ScissorState() : rect(-1, -1, -1, -1), enabled(false) {}

  nb::Rect<int> rect;

  bool enabled;
};

struct ClearColor {
  // Setup initial values as defined in the specification.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glClearColor.xml
  ClearColor() : red(0.0f), green(0.0f), blue(0.0f), alpha(0.0f) {}
  ClearColor(float red, float green, float blue, float alpha)
      : red(std::min(1.0f, std::max(0.0f, red))),
        green(std::min(1.0f, std::max(0.0f, green))),
        blue(std::min(1.0f, std::max(0.0f, blue))),
        alpha(std::min(1.0f, std::max(0.0f, alpha))) {}

  // Clear color properties setup by calls to glClearColor().
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glClearColor.xml
  float red;
  float green;
  float blue;
  float alpha;
};

// Represents the state set by glColorMask().
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glColorMask.xml
struct ColorMask {
  // Setup initial values as defined in the specification.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glColorMask.xml
  ColorMask() : red(true), green(true), blue(true), alpha(true) {}
  ColorMask(bool red, bool green, bool blue, bool alpha)
      : red(red), green(green), blue(blue), alpha(alpha) {}

  bool red;
  bool green;
  bool blue;
  bool alpha;
};

// This class is used to keep track of which uniform locations are dirty, or
// whether all of them are.
class DirtyUniforms {
 public:
  DirtyUniforms();

  // Returns true if the uniform at |location| is dirty.
  bool IsDirty(int location) const;

  // Returns true if any uniform is dirty.
  bool AnyDirty() const;

  // Clears the dirty flag from all uniforms.
  void ClearAll();

  // Marks all uniforms as being dirty.
  void MarkAll();

  // Marks the uniform at |location| as being dirty.
  void Mark(int location);

 private:
  std::vector<int> uniforms_dirty_;
  bool all_dirty_;
};

// The DrawState structure aggregates all GL state relevant to draw (or clear)
// commands.  It will be modified as GL ES commands are issued, but it will
// only be propagated to the platform-specific implementations when draw (or
// clear) calls are made.  A dirty flag is kept
struct DrawState {
  // The color the next color buffer clear will clear to.
  ClearColor clear_color;

  // Identifies which channels a draw (or clear) call is permitted to modify.
  ColorMask color_mask;

  // The current surface that draw (or clear) commands will target.
  egl::Surface* draw_surface;

  // The list of all active samplers that are available to the next draw call.
  EnabledTextureList textures;

  // The list of vertex attribute binding information for the next draw call.
  EnabledVertexAttributeList vertex_attributes;
  ConstantVertexAttributeList constant_vertex_attributes;

  // The scissor rectangle.  Draw calls should not modify pixels outside of
  // this.
  ScissorState scissor;

  // The viewport defines how normalized device coordinates should be
  // transformed to screen pixel coordinates.
  ViewportState viewport;

  // Defines how pixels produced by a draw call should be blended with the
  // existing pixels in the output framebuffer.
  BlendState blend_state;

  // Defines whether face culling is enabled, and upon which face if so.
  CullFaceState cull_face_state;

  // The currently bound array buffer, set by calling
  // glBindBuffer(GL_ARRAY_BUFFER).
  nb::scoped_refptr<Buffer> array_buffer;

  // The currently bound element array buffer, set by calling
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER).
  nb::scoped_refptr<Buffer> element_array_buffer;

  // The currently in-use Program object, set by a call to glUseProgram().
  nb::scoped_refptr<Program> used_program;

  // The currently bound framebuffer.  This is never NULL, even if the
  // default framebuffer is bound, in which case
  // framebuffer->color_attachment_surface() will be non-NULL and point to
  // the draw surface specified through eglMakeCurrent().
  nb::scoped_refptr<Framebuffer> framebuffer;
};

// The dirty flags structure tracks which draw state members have been modified
// since the last draw call was made.  This can be leveraged by implementations
// to avoid re-configurating draw state that has not changed.  Implementations
// should manually set these flags to false after they have been processed.
struct DrawStateDirtyFlags {
  DrawStateDirtyFlags() { MarkAll(); }

  void MarkAll() {
    clear_color_dirty = true;
    color_mask_dirty = true;
    draw_surface_dirty = true;
    textures_dirty = true;
    vertex_attributes_dirty = true;
    scissor_dirty = true;
    viewport_dirty = true;
    blend_state_dirty = true;
    cull_face_dirty = true;
    array_buffer_dirty = true;
    element_array_buffer_dirty = true;
    used_program_dirty = true;
    framebuffer_dirty = true;
    uniforms_dirty.MarkAll();
  }

  void MarkUsedProgram() {
    used_program_dirty = true;
    // Switching programs marks all uniforms, samplers and vertex attributes
    // as being dirty as well, since they are all properties of the program.
    vertex_attributes_dirty = true;
    textures_dirty = true;
    uniforms_dirty.MarkAll();
  }

  bool clear_color_dirty;
  bool color_mask_dirty;
  bool draw_surface_dirty;
  bool textures_dirty;
  bool vertex_attributes_dirty;
  bool scissor_dirty;
  bool viewport_dirty;
  bool blend_state_dirty;
  bool cull_face_dirty;
  bool array_buffer_dirty;
  bool element_array_buffer_dirty;
  bool used_program_dirty;
  bool framebuffer_dirty;

  // A list of uniform locations (within DrawState::used_program->uniforms())
  // whose values are dirty.
  DirtyUniforms uniforms_dirty;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_DRAW_STATE_H_
