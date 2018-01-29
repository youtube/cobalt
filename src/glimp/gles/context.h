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
#include "glimp/gles/framebuffer.h"
#include "glimp/gles/resource_manager.h"
#include "glimp/gles/sampler.h"
#include "glimp/gles/vertex_attribute.h"
#include "nb/ref_counted.h"
#include "nb/scoped_ptr.h"
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

  egl::Surface* draw_surface() {
    return default_draw_framebuffer_->color_attachment_surface();
  }

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

  void PixelStorei(GLenum pname, GLint param);

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
  void BlendEquation(GLenum mode);

  void CullFace(GLenum mode);
  void FrontFace(GLenum mode);

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
  void* MapBufferRange(GLenum target,
                       GLintptr offset,
                       GLsizeiptr length,
                       GLbitfield access);
  bool UnmapBuffer(GLenum target);

  void LineWidth(GLfloat width);

  void GenTextures(GLsizei n, GLuint* textures);
  void DeleteTextures(GLsizei n, const GLuint* textures);
  void ActiveTexture(GLenum texture);
  void BindTexture(GLenum target, GLuint texture);
  void GetTexParameteriv(GLenum target, GLenum pname, GLint* params);
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
  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const GLvoid* pixels);

  void GenFramebuffers(GLsizei n, GLuint* framebuffers);
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
  void BindFramebuffer(GLenum target, GLuint framebuffer);
  void FramebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            GLuint texture,
                            GLint level);
  GLenum CheckFramebufferStatus(GLenum target);
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer);

  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers);
  void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
  void BindRenderbuffer(GLenum target, GLuint renderbuffer);
  void RenderbufferStorage(GLenum target,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height);

  void StencilMask(GLuint mask);
  void ClearStencil(GLint s);

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
  void VertexAttribfv(GLuint indx, int elem_size, const GLfloat* values);

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

  void ReadPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  GLvoid* pixels);

  void Flush();
  void Finish();
  void SwapBuffers();

  // Called when eglBindTexImage() is called.
  bool BindTextureToEGLSurface(egl::Surface* surface);
  // Called when eglReleaseTexImage() is called.
  bool ReleaseTextureFromEGLSurface(egl::Surface* surface);

  // Utility functions for use by other modules.
  nb::scoped_refptr<Texture> GetTexture(uint32_t id) {
    return resource_manager_->GetTexture(id);
  }

  DrawStateDirtyFlags* GetDrawStateDirtyFlags() {
    return &draw_state_dirty_flags_;
  }

 private:
  void MakeCurrent(egl::Surface* draw, egl::Surface* read);
  void ReleaseContext();
  void SetError(GLenum error) { error_ = error; }

  // Returns the bound buffer for the specific specified target.
  // This returns a pointer because it is used by glBindBuffer() which modifies
  // what the returned scoped_refptr points to.
  nb::scoped_refptr<Buffer>* GetBoundBufferForTarget(GLenum target);

  // Returns the bound texture for the specific specified target and slot.
  // This returns a pointer because it is used by glBindTexture() which modifies
  // what the returned scoped_refptr points to.
  nb::scoped_refptr<Texture>* GetBoundTextureForTarget(GLenum target,
                                                       GLenum texture);

  void SetupExtensionsString();

  void UpdateVertexAttribsInDrawState();
  void UpdateSamplersInDrawState();

  // Packs enabled vertex attributes and samplers into dense lists in the
  // |draw_state_| if they have been modified.
  void CompressDrawStateForDrawCall();

  // Marks the used program as being dirty, but this may also imply the marking
  // of attributes and uniforms as being dirty as well.
  void MarkUsedProgramDirty();

  // Sets the bound framebuffer to the default framebuffer (e.g. when
  // glBindFramebuffer(GL_FRAMEBUFFER, 0) is called).
  void SetBoundDrawFramebufferToDefault();
  void SetBoundReadFramebufferToDefault();
  bool IsDefaultDrawFramebufferBound() const;
  bool IsDefaultReadFramebufferBound() const;

  // Takes settings like GL_UNPACK_ROW_LENGTH and GL_UNPACK_ALIGNMENT into
  // account to determine the pitch of incoming pixel data.
  int GetPitchForTextureData(int width, PixelFormat pixel_format) const;

  // A reference to the platform-specific implementation aspects of the context.
  nb::scoped_ptr<ContextImpl> impl_;

  // The thread that currently holds this context as its current context.
  SbThread current_thread_;

  // Has this context ever been made current before?
  bool has_been_current_;

  // The value to be returned when GetString(GL_EXTENSIONS) is called.
  std::string extensions_string_;

  // The resource manager containing all referenced resources.
  nb::scoped_refptr<ResourceManager> resource_manager_;

  // Sets the active texture, which can be thought of more intuitively as
  // the active "sampler".  Set using glActiveTexture().
  GLenum active_texture_;

  // The set of sampler units, of which |active_texture_| indexes.
  nb::scoped_array<nb::scoped_refptr<Texture> > texture_units_;
  bool enabled_textures_dirty_;

  // A mapping from an integer index (specified by the index parameter of
  // glBindAttribLocation(), glVertexAttribPointer(), and others) to vertex
  // attribute information structure.
  std::map<unsigned int, VertexAttributeArray> vertex_attrib_map_;

  // This map is populated by calls to glVertexAttribXfv() and contains
  // attribute values that, when used, should apply to ALL vertices in a draw
  // call.  These attribute values will be used instead of those set by
  // glVertexAttribPointer() whenever the corresponding vertex attribute id
  // (the key of this map) attribute array is disabled through a call to
  // glDisableVertexAttribArray().
  std::map<unsigned int, VertexAttributeConstant> const_vertex_attrib_map_;

  // Keeps track of which vertex attributes are enabled.  This set is modified
  // through calls to glEnableVertexAttribArray() and
  // glDisableVertexAttribArray().
  std::set<unsigned int> enabled_vertex_attribs_;
  bool enabled_vertex_attribs_dirty_;

  // The default draw and read framebuffer are those whose surfaces are set by
  // calls to eglMakeCurrent().  The default draw framebuffer is the initial
  // framebuffer target for draw commands, and can be selected by calling
  // glBindFramebuffer(0).
  nb::scoped_refptr<Framebuffer> default_draw_framebuffer_;
  nb::scoped_refptr<Framebuffer> default_read_framebuffer_;

  // The currently bound read framebuffer.  If this is set to the default read
  // framebuffer, then it will be equal to |default_read_framebuffer_|.
  nb::scoped_refptr<Framebuffer> read_framebuffer_;

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

  // The pack/unpack alignments are used when transferring pixel data to/from
  // client CPU memory, respectively.  For example, calls to glTexImage2D()
  // will refer to the unpack alignment to determine the expected alignment
  // of each row of pixel data.  These values are set through glPixelStorei().
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glPixelStorei.xml
  int pack_alignment_;
  int unpack_alignment_;

  // Allows the pitch of texture data to be explicitly specified.  This value
  // can be modified by calling glPixelStorei(GL_UNPACK_ALIGNMENT, x).
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glPixelStorei.xhtml
  int unpack_row_length_;

  // Tracks the currently bound pixel unpack buffer object, or NULL if none
  // are bound.
  nb::scoped_refptr<Buffer> bound_pixel_unpack_buffer_;

  // Keeps track of the set of EGLSurfaces that are bound to textures
  // currently.
  std::map<egl::Surface*, nb::scoped_refptr<Texture> > bound_egl_surfaces_;

  // The currently bound renderbuffer, specified through a call to
  // glBindRenderbuffer().
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBindRenderbuffer.xml
  nb::scoped_refptr<Renderbuffer> bound_renderbuffer_;

  // The last GL ES error raised.
  GLenum error_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_CONTEXT_H_
