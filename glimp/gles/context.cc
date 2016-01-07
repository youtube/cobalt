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

#include "glimp/gles/context.h"

#include <string>

#include "glimp/egl/error.h"
#include "glimp/egl/surface.h"
#include "starboard/log.h"
#include "starboard/once.h"

namespace glimp {
namespace gles {

namespace {

SbOnceControl s_tls_current_context_key_once_control = SB_ONCE_INITIALIZER;
SbThreadLocalKey s_tls_current_context_key = kSbThreadLocalKeyInvalid;

void InitializeThreadLocalKey() {
  s_tls_current_context_key = SbThreadCreateLocalKey(NULL);
}

SbThreadLocalKey GetThreadLocalKey() {
  SbOnce(&s_tls_current_context_key_once_control, &InitializeThreadLocalKey);
  return s_tls_current_context_key;
}

}  // namespace

Context::Context(nb::scoped_ptr<ContextImpl> context_impl,
                 Context* share_context)
    : impl_(context_impl.Pass()),
      current_thread_(kSbThreadInvalid),
      has_been_current_(false),
      active_texture_(GL_TEXTURE0),
      error_(GL_NO_ERROR) {
  if (share_context != NULL) {
    resource_manager_ = share_context->resource_manager_;
  } else {
    resource_manager_ = new ResourceManager();
  }

  SetupExtensionsString();
}

Context* Context::GetTLSCurrentContext() {
  return reinterpret_cast<Context*>(SbThreadGetLocalValue(GetThreadLocalKey()));
}

bool Context::SetTLSCurrentContext(Context* context,
                                   egl::Surface* draw,
                                   egl::Surface* read) {
  SB_DCHECK(context);
  SB_DCHECK(draw);
  SB_DCHECK(read);

  if (context->current_thread() != kSbThreadInvalid &&
      context->current_thread() != SbThreadGetCurrent()) {
    // Another thread currently holds the context current, and so it is an
    // error for us to try to make it current on this thread.
    egl::SetError(EGL_BAD_ACCESS);
    return false;
  }

  // If this thread currently has another context current, release that one
  // before we continue.
  Context* existing_context = GetTLSCurrentContext();
  if (existing_context != context) {
    if (existing_context) {
      existing_context->ReleaseContext();
    }
    SbThreadSetLocalValue(GetThreadLocalKey(),
                          reinterpret_cast<void*>(context));
  }

  context->MakeCurrent(draw, read);
  return true;
}

void Context::ReleaseTLSCurrentContext() {
  Context* existing_context = GetTLSCurrentContext();
  if (existing_context) {
    existing_context->ReleaseContext();
    SbThreadSetLocalValue(GetThreadLocalKey(), NULL);
  }
}

GLenum Context::GetError() {
  GLenum error = error_;
  error_ = GL_NO_ERROR;
  return error;
}

const GLubyte* Context::GetString(GLenum name) {
  switch (name) {
    case GL_EXTENSIONS: {
      return reinterpret_cast<const GLubyte*>(extensions_string_.c_str());
    } break;

    case GL_VENDOR:
    case GL_RENDERER:
    case GL_VERSION: {
      SB_NOTIMPLEMENTED();
    } break;

    default: { SetError(GL_INVALID_ENUM); }
  }
  return NULL;
}

GLuint Context::CreateProgram() {
  nb::scoped_ptr<ProgramImpl> program_impl = impl_->CreateProgram();
  SB_DCHECK(program_impl);

  nb::scoped_refptr<Program> program(new Program(program_impl.Pass()));

  return resource_manager_->RegisterProgram(program);
}

void Context::DeleteProgram(GLuint program) {
  // As indicated by the specification for glDeleteProgram(),
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDeleteProgram.xml
  // values of 0 will be silently ignored.
  if (program == 0) {
    return;
  }

  nb::scoped_refptr<Program> program_object =
      resource_manager_->DeregisterProgram(program);

  if (!program_object) {
    SetError(GL_INVALID_VALUE);
  }
}

void Context::AttachShader(GLuint program, GLuint shader) {
  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->GetShader(shader);
  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (!program_object->AttachShader(shader_object)) {
    // A shader of the given type was already attached.
    SetError(GL_INVALID_OPERATION);
  }
}

void Context::LinkProgram(GLuint program) {
  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  program_object->Link();
}

void Context::BindAttribLocation(
    GLuint program, GLuint index, const GLchar* name) {
  if (index >= GL_MAX_VERTEX_ATTRIBS) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (name[0] == 'g' && name[1] == 'l' && name[2] == '_') {
    // |name| is not allowed to begin with the reserved prefix, "gl_".
    SetError(GL_INVALID_OPERATION);
    return;
  }
  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  program_object->BindAttribLocation(index, name);
}

GLuint Context::CreateShader(GLenum type) {
  nb::scoped_ptr<ShaderImpl> shader_impl;
  if (type == GL_VERTEX_SHADER) {
    shader_impl = impl_->CreateVertexShader();
  } else if (type == GL_FRAGMENT_SHADER) {
    shader_impl = impl_->CreateFragmentShader();
  } else {
    SetError(GL_INVALID_ENUM);
    return 0;
  }
  SB_DCHECK(shader_impl);

  nb::scoped_refptr<Shader> shader(new Shader(shader_impl.Pass(), type));

  return resource_manager_->RegisterShader(shader);
}

void Context::DeleteShader(GLuint shader) {
  // As indicated by the specification for glDeleteShader(),
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDeleteShader.xml
  // values of 0 will be silently ignored.
  if (shader == 0) {
    return;
  }

  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->DeregisterShader(shader);

  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
  }
}

void Context::ShaderSource(GLuint shader,
                           GLsizei count,
                           const GLchar* const* string,
                           const GLint* length) {
  if (count < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->GetShader(shader);

  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  shader_object->ShaderSource(count, string, length);
}

void Context::CompileShader(GLuint shader) {
  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->GetShader(shader);

  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  shader_object->CompileShader();
}

void Context::GenBuffers(GLsizei n, GLuint* buffers) {
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    nb::scoped_ptr<BufferImpl> buffer_impl = impl_->CreateBuffer();
    SB_DCHECK(buffer_impl);

    nb::scoped_refptr<Buffer> buffer(new Buffer(buffer_impl.Pass()));

    buffers[i] = resource_manager_->RegisterBuffer(buffer);
  }
}

void Context::DeleteBuffers(GLsizei n, const GLuint* buffers) {
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    if (buffers[i] == 0) {
      // Silently ignore 0 buffers.
      continue;
    }

    nb::scoped_refptr<Buffer> buffer_object =
        resource_manager_->DeregisterBuffer(buffers[i]);

    if (!buffer_object) {
      // The specification does not indicate that any error should be set
      // in the case that there was an error deleting a specific buffer.
      //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDeleteBuffers.xml
      return;
    }

    // If a bound buffer is deleted, set the bound buffer to NULL.
    if (buffer_object->target_valid()) {
      *GetBoundBufferForTarget(buffer_object->target()) =
          nb::scoped_refptr<Buffer>();
    }
  }
}

void Context::BindBuffer(GLenum target, GLuint buffer) {
  if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (buffer == 0) {
    // Unbind the current buffer if 0 is passed in for buffer.
    *GetBoundBufferForTarget(target) = NULL;
    return;
  }

  nb::scoped_refptr<Buffer> buffer_object =
      resource_manager_->GetBuffer(buffer);

  if (!buffer_object) {
    // According to the specification, no error is generated if the buffer is
    // invalid.
    //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBindBuffer.xml
    return;
  }

  buffer_object->SetTarget(target);
  *GetBoundBufferForTarget(target) = buffer_object;
}

void Context::BufferData(GLenum target,
                         GLsizeiptr size,
                         const GLvoid* data,
                         GLenum usage) {
  if (size < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (usage != GL_STATIC_DRAW && usage != GL_DYNAMIC_DRAW) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  nb::scoped_refptr<Buffer> bound_buffer = *GetBoundBufferForTarget(target);
  if (bound_buffer == 0) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  bound_buffer->BufferData(size, data, usage);
}

void Context::MakeCurrent(egl::Surface* draw, egl::Surface* read) {
  SB_DCHECK(current_thread_ == kSbThreadInvalid ||
            current_thread_ == SbThreadGetCurrent());

  current_thread_ = SbThreadGetCurrent();
  impl_->SetDrawSurface(draw);
  impl_->SetReadSurface(read);

  if (!has_been_current_) {
    // According to the documentation for eglMakeCurrent(),
    //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglMakeCurrent.xhtml
    // we should set the scissor and viewport to the draw surface the first
    // time this context is made current.
    impl_->SetScissor(0, 0, draw->impl()->GetWidth(),
                      draw->impl()->GetHeight());
    impl_->SetViewport(0, 0, draw->impl()->GetWidth(),
                       draw->impl()->GetHeight());
    has_been_current_ = true;
  }
}

void Context::ReleaseContext() {
  SB_DCHECK(current_thread_ != kSbThreadInvalid);
  SB_DCHECK(current_thread_ == SbThreadGetCurrent());
  SB_DCHECK(has_been_current_);

  current_thread_ = kSbThreadInvalid;
}

nb::scoped_refptr<Buffer>* Context::GetBoundBufferForTarget(GLenum target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      return &bound_array_buffer_;
    case GL_ELEMENT_ARRAY_BUFFER:
      return &bound_element_array_buffer_;
  }

  SB_NOTREACHED();
  return NULL;
}

nb::scoped_refptr<Texture>* Context::GetBoundTextureForTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return &(samplers_[active_texture_].bound_texture);
    case GL_TEXTURE_CUBE_MAP:
      SB_NOTREACHED() << "Currently unimplemented in glimp.";
      return NULL;
  }

  SB_NOTREACHED();
  return NULL;
}

void Context::SetupExtensionsString() {
  // Extract the list of extensions from the platform-specific implementation
  // and then turn them into a string.
  ContextImpl::ExtensionList impl_extensions = impl_->GetExtensions();

  extensions_string_ = "";
  for (int i = 0; i < impl_extensions.size(); ++i) {
    if (i > 0) {
      extensions_string_ += " ";
    }
    extensions_string_ += impl_extensions[i];
  }

  // Since extensions_string_ will eventually be returned as an array of
  // unsigned chars, make sure that none of the characters in it are negative.
  for (int i = 0; i < extensions_string_.size(); ++i) {
    SB_DCHECK(extensions_string_[i] > 0);
  }
}

void Context::GenTextures(GLsizei n, GLuint* textures) {
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    nb::scoped_ptr<TextureImpl> texture_impl = impl_->CreateTexture();
    SB_DCHECK(texture_impl);

    nb::scoped_refptr<Texture> texture(new Texture(texture_impl.Pass()));

    textures[i] = resource_manager_->RegisterTexture(texture);
  }
}

void Context::DeleteTextures(GLsizei n, const GLuint* textures) {
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    if (textures[i] == 0) {
      // Silently ignore 0 textures.
      continue;
    }

    nb::scoped_refptr<Texture> texture_object =
        resource_manager_->DeregisterTexture(textures[i]);

    if (!texture_object) {
      // The specification does not indicate that any error should be set
      // in the case that there was an error deleting a specific texture.
      //   https://www.khronos.org/opengles/sdk/1.1/docs/man/glDeleteTextures.xml
      return;
    }

    // If a bound texture is deleted, set the bound texture to NULL.
    if (texture_object->target_valid()) {
      *GetBoundTextureForTarget(texture_object->target()) = NULL;
    }
  }
}

void Context::ActiveTexture(GLenum texture) {
  if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + kMaxActiveTextures) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  active_texture_ = texture;
}

void Context::BindTexture(GLenum target, GLuint texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (texture == 0) {
    // Unbind the current texture if 0 is passed in for texture.
    *GetBoundTextureForTarget(target) = NULL;
    return;
  }

  nb::scoped_refptr<Texture> texture_object =
      resource_manager_->GetTexture(texture);

  if (!texture_object) {
    // According to the specification, no error is generated if the texture is
    // invalid.
    //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBindTexture.xml
    return;
  }

  texture_object->SetTarget(target);
  *GetBoundTextureForTarget(target) = texture_object;
}

void Context::TexParameteri(GLenum target, GLenum pname, GLint param) {
  Sampler* active_sampler = &samplers_[active_texture_];

  switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
      active_sampler->mag_filter = param;
      break;
    case GL_TEXTURE_MIN_FILTER:
      active_sampler->min_filter = param;
      break;
    case GL_TEXTURE_WRAP_S:
      active_sampler->wrap_s = param;
      break;
    case GL_TEXTURE_WRAP_T:
      active_sampler->wrap_t = param;
      break;

    default:
      SetError(GL_INVALID_ENUM);
      return;
  }
}

void Context::TexImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid* pixels) {
  // TODO(***REMOVED***): Will implement in a future CL.
  SB_NOTIMPLEMENTED();
}

}  // namespace gles
}  // namespace glimp
