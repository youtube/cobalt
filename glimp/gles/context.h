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

  // Returns the thread that currently holds this Context, or kSbThreadInvalid
  // if no thread currently holds the context.
  SbThread current_thread() const { return current_thread_; }

  // Return the last error generated and reset the error flag to GL_NO_ERROR.
  GLenum GetError();

  const GLubyte* GetString(GLenum name);

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

  void DrawArrays(GLenum mode, GLint first, GLsizei count);

  void Flush();

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

  // The currently bound array buffer, set by calling
  // glBindBuffer(GL_ARRAY_BUFFER).
  nb::scoped_refptr<Buffer> bound_array_buffer_;

  // The currently bound element array buffer, set by calling
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER).
  nb::scoped_refptr<Buffer> bound_element_array_buffer_;

  // The currently in-use Program object, set by a call to glUseProgram().
  nb::scoped_refptr<Program> in_use_program_;

  // Sets the active texture, which can be thought of more intuitively as
  // the active "sampler".  Set using glActiveTexture().
  GLenum active_texture_;

  // The set of sampler units, of which |active_texture_| indexes.
  Sampler samplers_[kMaxActiveTextures];

  // A mapping from an integer index (specified by the index parameter of
  // glBindAttribLocation(), glVertexAttribPointer(), and others) to vertex
  // attribute information structure.
  std::map<unsigned int, VertexAttribute> vertex_attrib_map_;

  // Keeps track of which vertex attributes are enabled.  This set is modified
  // through calls to glEnableVertexAttribArray() and
  // glDisableVertexAttribArray().
  std::set<unsigned int> enabled_vertex_attribs_;

  // This vector maintains the set of enabled vertex attributes, and is passed
  // into the ContextImpl object on draw calls.  It is a dense set of only the
  // enabled vertex attributes.
  ContextImpl::EnabledVertexAttributeList draw_vertex_attribs_;

  // This vector maintains the set of enabled sampler objects, and is passed
  // into the ContextImpl object on draw calls.  It is a dense set of only the
  // enabled sampler units.
  ContextImpl::EnabledSamplerList draw_samplers_;

  // The last GL ES error raised.
  GLenum error_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_CONTEXT_H_
