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

#ifndef GLIMP_GLES_CONTEXT_H_
#define GLIMP_GLES_CONTEXT_H_

#include <GLES3/gl3.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "glimp/egl/surface.h"
#include "glimp/gles/context_impl.h"
#include "glimp/gles/draw_state.h"
#include "glimp/gles/resource_manager.h"
#include "glimp/gles/sampler.h"
#include "glimp/gles/vertex_attribute.h"
#include "glimp/nb/ref_counted.h"
#include "glimp/nb/scoped_ptr.h"
#include "starboard/thread.h"

namespace glimp {
namespace gles {

class Context {
 public:
  Context(nb::scoped_ptr<ContextImpl> context_impl, Context* share_context);

  // Returns current thread's current context, or NULL if nothing is current.
  static Context* GetTLSCurrentContext();

  // Sets the current thread's current context.  None of the parameters can
  // be NULL (use ReleaseTLSCurrentContext() if you wish to release the current
  // context).
  static bool SetTLSCurrentContext(Context* context,
                                   egl::Surface* draw,
                                   egl::Surface* read);

  // Releases the current thread's current context.
  static void ReleaseTLSCurrentContext();

  egl::Surface* draw_surface() { return draw_state_.draw_surface; }

  // Returns the thread that currently holds this Context, or kSbThreadInvalid
  // if no thread currently holds the context.
  SbThread current_thread() const { return current_thread_; }

  // Return the last error generated and reset the error flag to GL_NO_ERROR.
  GLenum GetError();

  const GLubyte* GetString(GLenum name);
  void GetIntegerv(GLenum pname, GLint* params);
  void GetShaderiv(GLuint shader, GLenum pname, GLint* params);
  void GetShaderInfoLog(GLuint shader,
                        GLsizei bufsize,
                        GLsizei* length,
                        GLchar* infolog);
  void GetProgramiv(GLuint program, GLenum pname, GLint* params);
  void GetProgramInfoLog(GLuint program,
                         GLsizei bufsize,
                         GLsizei* length,
                         GLchar* infolog);
  GLenum CheckFramebufferStatus(GLenum target);

  void Enable(GLenum cap);
  void Disable(GLenum cap);

  void ColorMask(GLboolean red,
                 GLboolean green,
                 GLboolean blue,
                 GLboolean alpha);
  void DepthMask(GLboolean flag);

  void Clear(GLbitfield mask);
  void ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);

  void BlendFunc(GLenum sfactor, GLenum dfactor);

  GLuint CreateProgram();
  void DeleteProgram(GLuint program);
  void AttachShader(GLuint program, GLuint shader);
  void LinkProgram(GLuint program);
  void BindAttribLocation(GLuint program, GLuint index, const GLchar* name);
  void UseProgram(GLuint program);

  GLuint CreateShader(GLenum type);
  void DeleteShader(GLuint shader);
  void ShaderSource(GLuint shader,
                    GLsizei count,
                    const GLchar* const* string,
                    const GLint* length);
  void CompileShader(GLuint shader);

  void GenBuffers(GLsizei n, GLuint* buffers);
  void DeleteBuffers(GLsizei n, const GLuint* buffers);
  void BindBuffer(GLenum target, GLuint buffer);
  void BufferData(GLenum target,
                  GLsizeiptr size,
                  const GLvoid* data,
                  GLenum usage);
  void BufferSubData(GLenum target,
                     GLintptr offset,
                     GLsizeiptr size,
                     const GLvoid* data);

  void GenTextures(GLsizei n, GLuint* textures);
  void DeleteTextures(GLsizei n, const GLuint* textures);
  void ActiveTexture(GLenum texture);
  void BindTexture(GLenum target, GLuint texture);
  void TexParameteri(GLenum target, GLenum pname, GLint param);
  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid* pixels);

  void BindFramebuffer(GLenum target, GLuint framebuffer);

  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height);

  void VertexAttribPointer(GLuint indx,
                           GLint size,
                           GLenum type,
                           GLboolean normalized,
                           GLsizei stride,
                           const GLvoid* ptr);
  void EnableVertexAttribArray(GLuint index);
  void DisableVertexAttribArray(GLuint index);

  GLint GetUniformLocation(GLuint program, const GLchar* name);
  void Uniformiv(GLint location,
                 GLsizei count,
                 GLsizei elem_size,
                 const GLint* v);
  void Uniformfv(GLint location,
                 GLsizei count,
                 GLsizei elem_size,
                 const GLfloat* v);
  void UniformMatrixfv(GLint location,
                       GLsizei count,
                       GLboolean transpose,
                       GLsizei dim_size,
                       const GLfloat* value);

  void DrawArrays(GLenum mode, GLint first, GLsizei count);
  void DrawElements(GLenum mode,
                    GLsizei count,
                    GLenum type,
                    const GLvoid* indices);

  void Flush();
  void SwapBuffers();

 private:
  static const int kMaxActiveTextures = 8;

  void MakeCurrent(egl::Surface* draw, egl::Surface* read);
  void ReleaseContext();
  void SetError(GLenum error) { error_ = error; }

  // Returns the bound buffer slot for the specific specified target.
  nb::scoped_refptr<Buffer>* GetBoundBufferForTarget(GLenum target);

  // Returns the bound texture slot for the specific specified target.
  nb::scoped_refptr<Texture>* GetBoundTextureForTarget(GLenum target);

  void SetupExtensionsString();

  void UpdateVertexAttribsInDrawState();
  void UpdateSamplersInDrawState();

  // Packs enabled vertex attributes and samplers into dense lists in the
  // |draw_state_| if they have been modified.
  void CompressDrawStateForDrawCall();

  // Marks the used program as being dirty, but this may also imply the marking
  // of attributes and uniforms as being dirty as well.
  void MarkUsedProgramDirty();

  // A reference to the platform-specific implementation aspects of the context.
  nb::scoped_ptr<ContextImpl> impl_;

  // The thread that currently holds this context as its current context.
  SbThread current_thread_;

  // Has this context ever been made current before?
  bool has_been_current_;

  // The current read surface.
  egl::Surface* read_surface_;

  // The value to be returned when GetString(GL_EXTENSIONS) is called.
  std::string extensions_string_;

  // The resource manager containing all referenced resources.
  nb::scoped_refptr<ResourceManager> resource_manager_;

  // Sets the active texture, which can be thought of more intuitively as
  // the active "sampler".  Set using glActiveTexture().
  GLenum active_texture_;

  // The set of sampler units, of which |active_texture_| indexes.
  Sampler samplers_[kMaxActiveTextures];
  bool enabled_samplers_dirty_;

  // A mapping from an integer index (specified by the index parameter of
  // glBindAttribLocation(), glVertexAttribPointer(), and others) to vertex
  // attribute information structure.
  std::map<unsigned int, VertexAttribute> vertex_attrib_map_;

  // Keeps track of which vertex attributes are enabled.  This set is modified
  // through calls to glEnableVertexAttribArray() and
  // glDisableVertexAttribArray().
  std::set<unsigned int> enabled_vertex_attribs_;
  bool enabled_vertex_attribs_dirty_;

  // Tracks all GL draw state.  It is updated by making various GL calls,
  // and it is read when a draw (or clear) call is made.  It is modified
  // by this Context object and read from the ContextImpl object.
  DrawState draw_state_;

  // Tracks which members of |draw_state_| have been modified since the last
  // draw (or clear) command issued to the ContextImpl object.  This
  // allows implementations to determine whether it is necessary to re-setup
  // certain context information.  It is expected that implementations will
  // set these dirty flags to false after they have processed the corresponding
  // draw state.
  DrawStateDirtyFlags draw_state_dirty_flags_;

  // The last GL ES error raised.
  GLenum error_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_CONTEXT_H_
