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
  nb::scoped_ptr<ProgramImpl> impl = impl_->CreateProgram();
  SB_DCHECK(impl);

  nb::scoped_refptr<Program> program(new Program(impl.Pass()));

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

GLuint Context::CreateShader(GLenum type) {
  nb::scoped_ptr<ShaderImpl> impl;
  if (type == GL_VERTEX_SHADER) {
    impl = impl_->CreateVertexShader();
  } else if (type == GL_FRAGMENT_SHADER) {
    impl = impl_->CreateFragmentShader();
  } else {
    SetError(GL_INVALID_ENUM);
    return 0;
  }
  SB_DCHECK(impl);

  nb::scoped_refptr<Shader> shader(new Shader(impl.Pass(), type));

  return resource_manager_->RegisterShader(shader);
}

void Context::DeleteShader(GLuint shader) {
  // As indicated by the specification for glDeleteShader(),
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDeleteShader.xml
  // values of 0 will be silently ignored.
  if (shader == 0) {
    return;
  }

  nb::scoped_refptr<Shader> shader_resource =
      resource_manager_->DeregisterShader(shader);

  if (!shader_resource) {
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

}  // namespace gles
}  // namespace glimp
