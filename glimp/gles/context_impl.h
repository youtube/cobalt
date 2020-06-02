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

#ifndef GLIMP_GLES_CONTEXT_IMPL_H_
#define GLIMP_GLES_CONTEXT_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "glimp/egl/surface.h"
#include "glimp/gles/buffer.h"
#include "glimp/gles/buffer_impl.h"
#include "glimp/gles/draw_mode.h"
#include "glimp/gles/draw_state.h"
#include "glimp/gles/index_data_type.h"
#include "glimp/gles/program.h"
#include "glimp/gles/program_impl.h"
#include "glimp/gles/sampler.h"
#include "glimp/gles/shader_impl.h"
#include "glimp/gles/texture_impl.h"
#include "glimp/gles/vertex_attribute.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

// The ContextImpl class is the interface to the platform-specific backend
// implementation of GL ES Context calls.  Many GL ES methods will ultimately
// end up calling a method on this class.  This class is also responsible for
// acting as a factory for the generation of resource Impl objects as well,
// such as textures or shaders.
class ContextImpl {
 public:
  // Type returned from GetExtensions() in response to
  // glGetString(GL_EXTENSIONS).
  typedef std::vector<std::string> ExtensionList;

  virtual ~ContextImpl() {}

  // Called via glGetString(GL_EXTENSIONS).
  // Note that glimp common code may append its own extensions to the list
  // provided by the implementation.
  //   https://www.khronos.org/opengles/sdk/1.1/docs/man/glGetString.xml
  virtual ExtensionList GetExtensions() const = 0;

  // Called via glGetIntegerv(GL_MAX_VERTEX_ATTRIBS).
  // Returns the maximum supported number of enabled vertex attributes.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxVertexAttribs() const = 0;

  // Called via glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS).
  // Returns the maximum supported number of textures per fragment shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxFragmentTextureUnits() const = 0;

  // Called via glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS).
  // Returns the maximum supported number of textures per vertex shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxVertexTextureImageUnits() const = 0;

  // Called via glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS).
  // Returns the maximum supported number of textures that can be used
  // to access textures from both vertex and fragment shaders combined.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxCombinedTextureImageUnits() const = 0;

  // Called via glGetIntegerv(GL_MAX_TEXTURE_SIZE).
  // Returns the maximum width and height of a texture, in pixels.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxTextureSize() const = 0;

  // Called via glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE).
  // Returns the maximum width and height of a render buffer, in pixels.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxRenderbufferSize() const = 0;

  // Called via glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS).
  // Returns the maximum number of uniforms that can be bound to a fragment
  // shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGet.xml
  virtual int GetMaxFragmentUniformVectors() const = 0;

  // Called via glCreateProgram.  Must create and return a platform-specific
  // ProgramImpl implementation.  If a NULL scoped_ptr is returned, it is
  // treated as an error.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCreateProgram.xml
  virtual nb::scoped_ptr<ProgramImpl> CreateProgram() = 0;

  // Called via glCreateShader(GL_VERTEX_SHADER).  Must create a
  // platform-specific ShaderImpl object representing a vertex shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCreateShader.xml
  virtual nb::scoped_ptr<ShaderImpl> CreateVertexShader() = 0;

  // Called via glCreateShader(GL_FRAGMENT_SHADER).  Must create a
  // platform-specific ShaderImpl object representing a fragment shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCreateShader.xml
  virtual nb::scoped_ptr<ShaderImpl> CreateFragmentShader() = 0;

  // Called via glGenBuffers().  Must create a platform-specific BufferImpl
  // object representing a buffer.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGenBuffers.xml
  virtual nb::scoped_ptr<BufferImpl> CreateBuffer() = 0;

  // Called via glGenTextures().  Must create a platform-specific TextureImpl
  // object representing a texture.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGenTextures.xml
  virtual nb::scoped_ptr<TextureImpl> CreateTexture() = 0;

  // Called when glFlush() is called.  After this method is called, client
  // code may assume that all previously issued graphics commands will
  // eventually be executed by the GPU.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glFlush.xml
  virtual void Flush() = 0;

  // Called when glFinish() is called.  After this method is called, client
  // code can assume that all previously issued GL commands have been submitted
  // and also have finished being processed by the GPU.
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glFinish.xhtml
  virtual void Finish() = 0;

  // Called when glClear() is called.  Clears all of the three buffers that have
  // their |clear_*| parameters set.
  //   |draw_state| represents the current GL ES draw state at the time of this
  //                call.
  //   |dirty_flags| represents which members of the |draw_state| have been
  //                 modified since the last draw (or clear) call.  This can
  //                 be leveraged to avoid reconfiguring unchanged state.
  //                 It is expected that implementations will manually set
  //                 these flags to false after they have processed the
  //                 corresponding draw state member.
  //   https://www.opengl.org/sdk/docs/man2/xhtml/glClear.xml
  virtual void Clear(bool clear_color,
                     bool clear_depth,
                     bool clear_stencil,
                     const DrawState& draw_state,
                     DrawStateDirtyFlags* dirty_flags) = 0;

  // Called when glDrawArrays() is called.  This method must generate GPU
  // commands to render the passed in |draw_state.array_buffer| whose structure
  // is defined by |draw_state.attributes|.  The vertex program
  // |draw_state.used_program| should be used to render the vertices, and
  // |draw_state.samplers| defines the set of textures to be bound for
  // the draw call.
  //   |draw_state| represents the current GL ES draw state at the time of this
  //                call.
  //   |dirty_flags| represents which members of the |draw_state| have been
  //                 modified since the last draw (or clear) call.  This can
  //                 be leveraged to avoid reconfiguring unchanged state.
  //                 It is expected that implementations will manually set
  //                 these flags to false after they have processed the
  //                 corresponding draw state member.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDrawArrays.xml
  virtual void DrawArrays(DrawMode mode,
                          int first_vertex,
                          int num_vertices,
                          const DrawState& draw_state,
                          DrawStateDirtyFlags* dirty_flags) = 0;

  // Called when glDrawElements() is called.  This method must generate GPU
  // commands to render the passed in |draw_state.array_buffer| whose structure
  // is defined by |draw_state.attributes|.  The particular vertices rendered
  // are chosen by the indices defined within |draw_state.element_array_buffer|
  // + |intptr_t index_offset_in_bytes|.  The vertex program
  // |draw_state.used_program| should be used to render the vertices, and
  // |draw_state.samplers| defines the set of textures to be bound for the draw
  // call.
  //   |draw_state| represents the current GL ES draw state at the time of this
  //                call.
  //   |dirty_flags| represents which members of the |draw_state| have been
  //                 modified since the last draw (or clear) call.  This can
  //                 be leveraged to avoid reconfiguring unchanged state.
  //                 It is expected that implementations will manually set
  //                 these flags to false after they have processed the
  //                 corresponding draw state member.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDrawElements.xml
  virtual void DrawElements(DrawMode mode,
                            int num_vertices,
                            IndexDataType index_data_type,
                            intptr_t index_offset_in_bytes,
                            const DrawState& draw_state,
                            DrawStateDirtyFlags* dirty_flags) = 0;

  // Called when eglSwapBuffers() is called.  This method is responsible for
  // submitting a command to the GPU to indicate that we are done rendering this
  // frame and that the current draw surface should be swapped to the display.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglSwapBuffers.xhtml
  virtual void SwapBuffers(egl::Surface* surface) = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_CONTEXT_IMPL_H_
