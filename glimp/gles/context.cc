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
#include "glimp/gles/blend_state.h"
#include "glimp/gles/cull_face_state.h"
#include "glimp/gles/draw_mode.h"
#include "glimp/gles/index_data_type.h"
#include "glimp/gles/pixel_format.h"
#include "glimp/tracing/tracing.h"
#include "nb/pointer_arithmetic.h"
#include "starboard/log.h"
#include "starboard/memory.h"
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
      enabled_textures_dirty_(true),
      enabled_vertex_attribs_dirty_(true),
      pack_alignment_(4),
      unpack_alignment_(4),
      unpack_row_length_(0),
      error_(GL_NO_ERROR) {
  if (share_context != NULL) {
    resource_manager_ = share_context->resource_manager_;
  } else {
    resource_manager_ = new ResourceManager();
  }

  SetupExtensionsString();

  texture_units_.reset(
      new nb::scoped_refptr<Texture>[impl_->GetMaxFragmentTextureUnits()]);
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
    SB_DLOG(WARNING) << "Another thread holds current the context that is to "
                        "be made current on this thread.";
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  GLenum error = error_;
  error_ = GL_NO_ERROR;
  return error;
}

const GLubyte* Context::GetString(GLenum name) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (name) {
    case GL_EXTENSIONS:
      return reinterpret_cast<const GLubyte*>(extensions_string_.c_str());
    case GL_VERSION:
      return reinterpret_cast<const GLubyte*>("OpenGL ES 2.0 (glimp)");
    case GL_VENDOR:
      return reinterpret_cast<const GLubyte*>("Google Inc.");
    case GL_RENDERER:
      return reinterpret_cast<const GLubyte*>("glimp");
    case GL_SHADING_LANGUAGE_VERSION:
      return reinterpret_cast<const GLubyte*>("OpenGL ES GLSL ES 1.00");

    default: {
      SetError(GL_INVALID_ENUM);
      return NULL;
    }
  }
}

void Context::GetIntegerv(GLenum pname, GLint* params) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
      *params = impl_->GetMaxTextureSize();
      break;
    case GL_ACTIVE_TEXTURE:
      *params = static_cast<GLint>(active_texture_);
      break;
    case GL_MAX_RENDERBUFFER_SIZE:
      *params = impl_->GetMaxRenderbufferSize();
      break;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      // We don't currently support compressed textures.
      *params = 0;
      break;
    case GL_MAX_VERTEX_ATTRIBS:
      *params = impl_->GetMaxVertexAttribs();
      break;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      *params = impl_->GetMaxFragmentTextureUnits();
      break;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      *params = impl_->GetMaxFragmentUniformVectors();
      break;
    default: {
      SB_NOTIMPLEMENTED();
      SetError(GL_INVALID_ENUM);
    }
  }
}

void Context::GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->GetShader(shader);
  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  GLenum result = shader_object->GetShaderiv(pname, params);
  if (result != GL_NO_ERROR) {
    SetError(result);
  }
}

void Context::GetShaderInfoLog(GLuint shader,
                               GLsizei bufsize,
                               GLsizei* length,
                               GLchar* infolog) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (bufsize < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->GetShader(shader);
  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  shader_object->GetShaderInfoLog(bufsize, length, infolog);
}

void Context::GetProgramiv(GLuint program, GLenum pname, GLint* params) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  GLenum result = program_object->GetProgramiv(pname, params);
  if (result != GL_NO_ERROR) {
    SetError(result);
  }
}

void Context::GetProgramInfoLog(GLuint program,
                                GLsizei bufsize,
                                GLsizei* length,
                                GLchar* infolog) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (bufsize < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  program_object->GetProgramInfoLog(bufsize, length, infolog);
}

void Context::PixelStorei(GLenum pname, GLint param) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (pname) {
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (param != 1 && param != 4 && param != 8) {
        SetError(GL_INVALID_VALUE);
        return;
      }
      break;

    default:
      if (param < 0) {
        SetError(GL_INVALID_VALUE);
        return;
      }
      break;
  }

  switch (pname) {
    case GL_PACK_ALIGNMENT:
      pack_alignment_ = param;
      break;
    case GL_UNPACK_ALIGNMENT:
      unpack_alignment_ = param;
      break;
    case GL_UNPACK_ROW_LENGTH:
      unpack_row_length_ = param;
      break;
    case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_ROWS:
    case GL_PACK_SKIP_PIXELS:
    case GL_UNPACK_IMAGE_HEIGHT:
    case GL_UNPACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_IMAGES:
      SB_NOTIMPLEMENTED();
    default:
      SetError(GL_INVALID_ENUM);
      break;
  }
}

void Context::Enable(GLenum cap) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (cap) {
    case GL_BLEND:
      draw_state_.blend_state.enabled = true;
      draw_state_dirty_flags_.blend_state_dirty = true;
      break;
    case GL_SCISSOR_TEST:
      draw_state_.scissor.enabled = true;
      draw_state_dirty_flags_.scissor_dirty = true;
      break;
    case GL_CULL_FACE:
      draw_state_.cull_face_state.enabled = true;
      draw_state_.cull_face_state.mode = CullFaceState::kBack;
      draw_state_dirty_flags_.cull_face_dirty = true;
      break;
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_STENCIL_TEST:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
      SB_NOTIMPLEMENTED();
    default:
      SetError(GL_INVALID_ENUM);
  }
}

void Context::Disable(GLenum cap) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (cap) {
    case GL_BLEND:
      draw_state_.blend_state.enabled = false;
      draw_state_dirty_flags_.blend_state_dirty = true;
      break;
    case GL_SCISSOR_TEST:
      draw_state_.scissor.enabled = false;
      draw_state_dirty_flags_.scissor_dirty = true;
      break;
    case GL_CULL_FACE:
      draw_state_.cull_face_state.enabled = false;
      draw_state_dirty_flags_.cull_face_dirty = true;
      break;
    case GL_DEPTH_TEST:
    case GL_DITHER:
    case GL_STENCIL_TEST:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_COVERAGE:
      // Since these are not implemented yet, it is not an error to do nothing
      // when we ask for them to be disabled!
      SB_NOTIMPLEMENTED();
      break;
    default:
      SetError(GL_INVALID_ENUM);
  }
}

void Context::ColorMask(GLboolean red,
                        GLboolean green,
                        GLboolean blue,
                        GLboolean alpha) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  draw_state_.color_mask = gles::ColorMask(red, green, blue, alpha);
  draw_state_dirty_flags_.color_mask_dirty = true;
}

void Context::DepthMask(GLboolean flag) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (flag == GL_TRUE) {
    SB_NOTIMPLEMENTED() << "glimp currently does not support depth buffers.";
    SetError(GL_INVALID_OPERATION);
  }
}

void Context::Clear(GLbitfield mask) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  impl_->Clear(mask & GL_COLOR_BUFFER_BIT, mask & GL_DEPTH_BUFFER_BIT,
               mask & GL_STENCIL_BUFFER_BIT, draw_state_,
               &draw_state_dirty_flags_);
}

void Context::ClearColor(GLfloat red,
                         GLfloat green,
                         GLfloat blue,
                         GLfloat alpha) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  draw_state_.clear_color = gles::ClearColor(red, green, blue, alpha);
  draw_state_dirty_flags_.clear_color_dirty = true;
}

namespace {
BlendState::Factor BlendStateFactorFromGLenum(GLenum blend_factor) {
  switch (blend_factor) {
    case GL_ZERO:
      return BlendState::kFactorZero;
    case GL_ONE:
      return BlendState::kFactorOne;
    case GL_SRC_COLOR:
      return BlendState::kFactorSrcColor;
    case GL_ONE_MINUS_SRC_COLOR:
      return BlendState::kFactorOneMinusSrcColor;
    case GL_DST_COLOR:
      return BlendState::kFactorDstColor;
    case GL_ONE_MINUS_DST_COLOR:
      return BlendState::kFactorOneMinusDstColor;
    case GL_SRC_ALPHA:
      return BlendState::kFactorSrcAlpha;
    case GL_ONE_MINUS_SRC_ALPHA:
      return BlendState::kFactorOneMinusSrcAlpha;
    case GL_DST_ALPHA:
      return BlendState::kFactorDstAlpha;
    case GL_ONE_MINUS_DST_ALPHA:
      return BlendState::kFactorOneMinusDstAlpha;
    case GL_CONSTANT_COLOR:
      return BlendState::kFactorConstantColor;
    case GL_ONE_MINUS_CONSTANT_COLOR:
      return BlendState::kFactorOneMinusConstantColor;
    case GL_CONSTANT_ALPHA:
      return BlendState::kFactorConstantAlpha;
    case GL_ONE_MINUS_CONSTANT_ALPHA:
      return BlendState::kFactorOneMinusConstantAlpha;
    case GL_SRC_ALPHA_SATURATE:
      return BlendState::kFactorSrcAlphaSaturate;
    default:
      return BlendState::kFactorInvalid;
  }
}
}  // namespace

void Context::BlendFunc(GLenum sfactor, GLenum dfactor) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  BlendState::Factor src_factor = BlendStateFactorFromGLenum(sfactor);
  BlendState::Factor dst_factor = BlendStateFactorFromGLenum(dfactor);
  if (src_factor == BlendState::kFactorInvalid ||
      dst_factor == BlendState::kFactorInvalid) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  draw_state_.blend_state.src_factor = src_factor;
  draw_state_.blend_state.dst_factor = dst_factor;
  draw_state_dirty_flags_.blend_state_dirty = true;
}

namespace {
CullFaceState::Mode CullFaceModeFromEnum(GLenum mode) {
  switch (mode) {
    case GL_FRONT:
      return CullFaceState::kFront;
    case GL_BACK:
      return CullFaceState::kBack;
    case GL_FRONT_AND_BACK:
      return CullFaceState::kFrontAndBack;
    default:
      return CullFaceState::kModeInvalid;
  }
}
}  // namespace

void Context::CullFace(GLenum mode) {
  CullFaceState::Mode cull_face_mode = CullFaceModeFromEnum(mode);
  if (cull_face_mode == CullFaceState::kModeInvalid) {
    SetError(GL_INVALID_ENUM);
    return;
  }
  draw_state_.cull_face_state.mode = cull_face_mode;
  draw_state_dirty_flags_.cull_face_dirty = true;
}

GLuint Context::CreateProgram() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  nb::scoped_ptr<ProgramImpl> program_impl = impl_->CreateProgram();
  SB_DCHECK(program_impl);

  nb::scoped_refptr<Program> program(new Program(program_impl.Pass()));

  return resource_manager_->RegisterProgram(program);
}

void Context::DeleteProgram(GLuint program) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  program_object->Link();

  if (program_object.get() == draw_state_.used_program.get()) {
    MarkUsedProgramDirty();
  }
}

void Context::BindAttribLocation(GLuint program,
                                 GLuint index,
                                 const GLchar* name) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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

  if (program_object.get() == draw_state_.used_program.get()) {
    draw_state_dirty_flags_.vertex_attributes_dirty = true;
  }
}

void Context::UseProgram(GLuint program) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (program == 0) {
    draw_state_.used_program = NULL;
    MarkUsedProgramDirty();
    return;
  }

  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (!program_object->linked()) {
    // Only linked programs can be used.
    SetError(GL_INVALID_OPERATION);
    return;
  }

  if (program_object.get() != draw_state_.used_program.get()) {
    draw_state_.used_program = program_object;
    MarkUsedProgramDirty();
  }
}

GLuint Context::CreateShader(GLenum type) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  nb::scoped_refptr<Shader> shader_object =
      resource_manager_->GetShader(shader);

  if (!shader_object) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  shader_object->CompileShader();
}

void Context::GenBuffers(GLsizei n, GLuint* buffers) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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

    if (buffer_object->is_mapped()) {
      // Buffer objects should be unmapped if they are deleted.
      //   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
      buffer_object->Unmap();
    }

    // If a bound buffer is deleted, set the bound buffer to NULL. The buffer
    // may be bound to any target, therefore we must scan them all.
    const GLenum buffer_targets[3] = {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
                                      GL_PIXEL_UNPACK_BUFFER};
    for (int target_index = 0; target_index < SB_ARRAY_SIZE(buffer_targets);
         ++target_index) {
      GLenum target = buffer_targets[target_index];
      nb::scoped_refptr<Buffer>* bound_buffer = GetBoundBufferForTarget(target);
      SB_DCHECK(bound_buffer);
      if ((*bound_buffer).get() == buffer_object.get()) {
        *bound_buffer = NULL;
      }
    }
  }
}

namespace {
bool IsValidBufferTarget(GLenum target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
      return true;
      break;
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
      SB_NOTIMPLEMENTED() << "Buffer target " << target << " is not supported "
                                                           "in glimp.";
    default:
      return false;
  }
}
}  // namespace

void Context::BindBuffer(GLenum target, GLuint buffer) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (!IsValidBufferTarget(target)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  nb::scoped_refptr<Buffer>* bound_buffer = GetBoundBufferForTarget(target);
  SB_DCHECK(bound_buffer);
  nb::scoped_refptr<Buffer> buffer_object;
  if (buffer != 0) {
    buffer_object = resource_manager_->GetBuffer(buffer);
    if (!buffer_object) {
      // The buffer to be bound is invalid.
      SB_NOTIMPLEMENTED()
          << "Creating buffers with glBindBuffer () not supported";
      return;
    }
  }

  *bound_buffer = buffer_object;
}

void Context::BufferData(GLenum target,
                         GLsizeiptr size,
                         const GLvoid* data,
                         GLenum usage) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (size < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (usage != GL_STREAM_DRAW && usage != GL_STATIC_DRAW &&
      usage != GL_DYNAMIC_DRAW) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (!IsValidBufferTarget(target)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  nb::scoped_refptr<Buffer> bound_buffer = *GetBoundBufferForTarget(target);
  if (bound_buffer == 0) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  if (bound_buffer->is_mapped()) {
    // According to the specification, we must unmap the buffer if its data
    // store is recreated with glBufferData.
    //   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
    bound_buffer->Unmap();
  }

  if (!bound_buffer->Allocate(usage, size)) {
    SetError(GL_OUT_OF_MEMORY);
    return;
  }

  if (data) {
    if (!bound_buffer->SetData(0, size, data)) {
      SetError(GL_OUT_OF_MEMORY);
      return;
    }
  }
}

void Context::BufferSubData(GLenum target,
                            GLintptr offset,
                            GLsizeiptr size,
                            const GLvoid* data) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (size < 0 || offset < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (!IsValidBufferTarget(target)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  nb::scoped_refptr<Buffer> bound_buffer = *GetBoundBufferForTarget(target);
  if (bound_buffer == 0) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  if (offset + size > bound_buffer->size_in_bytes()) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (bound_buffer->is_mapped()) {
    // According to the specification, we must unmap the buffer if its data
    // store is recreated with glBufferData.
    //   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
    bound_buffer->Unmap();
  }

  // Nothing in the specification says there should be an error if data
  // is NULL.
  if (data) {
    if (!bound_buffer->SetData(offset, size, data)) {
      SetError(GL_OUT_OF_MEMORY);
      return;
    }
  }
}

namespace {
// This function is based off of the logic described in the "Errors" section
// of the specification:
//   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
bool MapBufferRangeAccessFlagsAreValid(GLbitfield access) {
  if (access &
      ~(GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
        GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
        GL_MAP_UNSYNCHRONIZED_BIT)) {
    return false;
  }

  if (!(access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))) {
    return false;
  }

  if ((access & GL_MAP_READ_BIT) &&
      (access & (GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
                 GL_MAP_UNSYNCHRONIZED_BIT))) {
    return false;
  }

  if ((access & GL_MAP_FLUSH_EXPLICIT_BIT) && !(access & GL_MAP_WRITE_BIT)) {
    return false;
  }

  return true;
}
}  // namespace

void* Context::MapBufferRange(GLenum target,
                              GLintptr offset,
                              GLsizeiptr length,
                              GLbitfield access) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (!IsValidBufferTarget(target)) {
    SetError(GL_INVALID_ENUM);
    return NULL;
  }

  nb::scoped_refptr<Buffer> bound_buffer = *GetBoundBufferForTarget(target);
  if (bound_buffer == 0) {
    SetError(GL_INVALID_OPERATION);
    return NULL;
  }

  if (offset < 0 || length < 0 ||
      offset + length > bound_buffer->size_in_bytes()) {
    SetError(GL_INVALID_VALUE);
    return NULL;
  }

  if (bound_buffer->is_mapped()) {
    SetError(GL_INVALID_OPERATION);
    return NULL;
  }

  if (!MapBufferRangeAccessFlagsAreValid(access)) {
    SetError(GL_INVALID_OPERATION);
    return NULL;
  }

  SB_DCHECK(access & GL_MAP_INVALIDATE_BUFFER_BIT)
      << "glimp requires the GL_MAP_INVALIDATE_BUFFER_BIT flag to be set.";
  SB_DCHECK(access & GL_MAP_UNSYNCHRONIZED_BIT)
      << "glimp requres the GL_MAP_UNSYNCHRONIZED_BIT flag to be set.";
  SB_DCHECK(!(access & GL_MAP_FLUSH_EXPLICIT_BIT))
      << "glimp does not support the GL_MAP_FLUSH_EXPLICIT_BIT flag.";
  SB_DCHECK(length == bound_buffer->size_in_bytes())
      << "glimp only supports mapping the entire buffer.";

  void* mapped = bound_buffer->Map();
  if (!mapped) {
    SetError(GL_OUT_OF_MEMORY);
  }

  return mapped;
}

bool Context::UnmapBuffer(GLenum target) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (!IsValidBufferTarget(target)) {
    SetError(GL_INVALID_ENUM);
    return GL_FALSE;
  }

  nb::scoped_refptr<Buffer> bound_buffer = *GetBoundBufferForTarget(target);
  if (bound_buffer == 0) {
    SetError(GL_INVALID_OPERATION);
    return GL_FALSE;
  }

  if (bound_buffer->is_mapped()) {
    return bound_buffer->Unmap();
  } else {
    // The specification is unclear on what to do in the case where the buffer
    // was not mapped to begin with, so we return GL_FALSE in this case.
    //   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
    return GL_FALSE;
  }
}

void Context::MakeCurrent(egl::Surface* draw, egl::Surface* read) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(current_thread_ == kSbThreadInvalid ||
            current_thread_ == SbThreadGetCurrent());

  current_thread_ = SbThreadGetCurrent();

  if (!has_been_current_) {
    // According to the documentation for eglMakeCurrent(),
    //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglMakeCurrent.xhtml
    // we should set the scissor and viewport to the draw surface the first
    // time this context is made current.
    Scissor(0, 0, draw->impl()->GetWidth(), draw->impl()->GetHeight());
    Viewport(0, 0, draw->impl()->GetWidth(), draw->impl()->GetHeight());

    // Setup the default framebuffers and bind them.
    SB_DCHECK(!default_draw_framebuffer_);
    SB_DCHECK(!default_read_framebuffer_);
    SB_DCHECK(!draw_state_.framebuffer);
    default_draw_framebuffer_ = new Framebuffer(draw);
    default_read_framebuffer_ = new Framebuffer(read);
    draw_state_.framebuffer = default_draw_framebuffer_;
    read_framebuffer_ = default_read_framebuffer_;

    has_been_current_ = true;
  }

  // Update our draw and read framebuffers, marking the framebuffer dirty
  // flag if the default framebuffer is the one that is currently bound.
  if (default_draw_framebuffer_->color_attachment_surface() != draw) {
    default_draw_framebuffer_->UpdateColorSurface(draw);
    if (draw_state_.framebuffer == default_draw_framebuffer_) {
      draw_state_dirty_flags_.framebuffer_dirty = true;
    }
  }
  if (default_read_framebuffer_->color_attachment_surface() != read) {
    default_read_framebuffer_->UpdateColorSurface(read);
  }
}

void Context::ReleaseContext() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(current_thread_ != kSbThreadInvalid);
  SB_DCHECK(current_thread_ == SbThreadGetCurrent());
  SB_DCHECK(has_been_current_);

  current_thread_ = kSbThreadInvalid;
}

nb::scoped_refptr<Buffer>* Context::GetBoundBufferForTarget(GLenum target) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (target) {
    case GL_ARRAY_BUFFER:
      draw_state_dirty_flags_.array_buffer_dirty = true;
      return &draw_state_.array_buffer;
    case GL_ELEMENT_ARRAY_BUFFER:
      draw_state_dirty_flags_.element_array_buffer_dirty = true;
      return &draw_state_.element_array_buffer;
    case GL_PIXEL_UNPACK_BUFFER:
      return &bound_pixel_unpack_buffer_;
  }

  SB_NOTREACHED();
  return NULL;
}

nb::scoped_refptr<Texture>* Context::GetBoundTextureForTarget(GLenum target,
                                                              GLenum texture) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  switch (target) {
    case GL_TEXTURE_2D:
      return &(texture_units_[texture - GL_TEXTURE0]);
    case GL_TEXTURE_CUBE_MAP:
      SB_NOTREACHED() << "Currently unimplemented in glimp.";
      return NULL;
  }

  SB_NOTREACHED();
  return NULL;
}

void Context::SetupExtensionsString() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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
  GLIMP_TRACE_EVENT0(__FUNCTION__);
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

    // If a bound texture is deleted, set the bound texture to NULL. The texture
    // may be bound to multiple texture units, including texture units that are
    // not active, therefore we must scan them all.
    for (int texture_index = 0;
         texture_index < impl_->GetMaxFragmentTextureUnits(); ++texture_index) {
      GLenum texture_unit = texture_index + GL_TEXTURE0;
      nb::scoped_refptr<Texture>* bound_texture =
          GetBoundTextureForTarget(GL_TEXTURE_2D, texture_unit);
      if ((*bound_texture).get() == texture_object.get()) {
        enabled_textures_dirty_ = true;
        *bound_texture = NULL;
      }
    }
  }
}

void Context::ActiveTexture(GLenum texture) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (texture < GL_TEXTURE0 ||
      texture >= GL_TEXTURE0 + impl_->GetMaxFragmentTextureUnits()) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  active_texture_ = texture;
}

void Context::BindTexture(GLenum target, GLuint texture) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  nb::scoped_refptr<Texture>* bound_texture =
      GetBoundTextureForTarget(target, active_texture_);
  SB_DCHECK(bound_texture);
  nb::scoped_refptr<Texture> texture_object;
  if (texture != 0) {
    texture_object = resource_manager_->GetTexture(texture);
    if (!texture_object) {
      // The texture to be bound is invalid.
      SB_NOTIMPLEMENTED()
          << "Creating textures with glBindTexture() not supported";
      return;
    }
  }

  if ((*bound_texture).get() == texture_object.get()) {
    // The new texture being bound is the same as the already the bound
    // texture.
    return;
  }
  *bound_texture = texture_object;
  enabled_textures_dirty_ = true;
}

namespace {
Sampler::MinFilter MinFilterFromGLEnum(GLenum min_filter) {
  switch (min_filter) {
    case GL_NEAREST:
      return Sampler::kMinFilterNearest;
    case GL_LINEAR:
      return Sampler::kMinFilterLinear;
    case GL_NEAREST_MIPMAP_NEAREST:
      return Sampler::kMinFilterNearestMipMapNearest;
    case GL_NEAREST_MIPMAP_LINEAR:
      return Sampler::kMinFilterNearestMipMapLinear;
    case GL_LINEAR_MIPMAP_NEAREST:
      return Sampler::kMinFilterLinearMipMapNearest;
    case GL_LINEAR_MIPMAP_LINEAR:
      return Sampler::kMinFilterLinearMipMapLinear;
    default:
      return Sampler::kMinFilterInvalid;
  }
}

Sampler::MagFilter MagFilterFromGLEnum(GLenum mag_filter) {
  switch (mag_filter) {
    case GL_NEAREST:
      return Sampler::kMagFilterNearest;
    case GL_LINEAR:
      return Sampler::kMagFilterLinear;
    default:
      return Sampler::kMagFilterInvalid;
  }
}

Sampler::WrapMode WrapModeFromGLEnum(GLenum wrap_mode) {
  switch (wrap_mode) {
    case GL_CLAMP_TO_EDGE:
      return Sampler::kWrapModeClampToEdge;
    case GL_MIRRORED_REPEAT:
      return Sampler::kWrapModeMirroredRepeat;
    case GL_REPEAT:
      return Sampler::kWrapModeRepeat;
    default:
      return Sampler::kWrapModeInvalid;
  }
}

GLenum GLEnumFromMinFilter(Sampler::MinFilter min_filter) {
  switch (min_filter) {
    case Sampler::kMinFilterNearest:
      return GL_NEAREST;
    case Sampler::kMinFilterLinear:
      return GL_LINEAR;
    case Sampler::kMinFilterNearestMipMapNearest:
      return GL_NEAREST_MIPMAP_NEAREST;
    case Sampler::kMinFilterNearestMipMapLinear:
      return GL_NEAREST_MIPMAP_LINEAR;
    case Sampler::kMinFilterLinearMipMapNearest:
      return GL_LINEAR_MIPMAP_NEAREST;
    case Sampler::kMinFilterLinearMipMapLinear:
      return GL_LINEAR_MIPMAP_LINEAR;
    default: {
      SB_NOTREACHED();
      return GL_LINEAR;
    }
  }
}

GLenum GLEnumFromMagFilter(Sampler::MagFilter mag_filter) {
  switch (mag_filter) {
    case Sampler::kMagFilterNearest:
      return GL_NEAREST;
    case Sampler::kMagFilterLinear:
      return GL_LINEAR;
    default: {
      SB_NOTREACHED();
      return GL_LINEAR;
    }
  }
}

GLenum GLEnumFromWrapMode(Sampler::WrapMode wrap_mode) {
  switch (wrap_mode) {
    case Sampler::kWrapModeClampToEdge:
      return GL_CLAMP_TO_EDGE;
    case Sampler::kWrapModeMirroredRepeat:
      return GL_MIRRORED_REPEAT;
    case Sampler::kWrapModeRepeat:
      return GL_REPEAT;
    default: {
      SB_NOTREACHED();
      return GL_REPEAT;
    }
  }
}
}  // namespace

void Context::GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  Sampler* active_sampler = (*GetBoundTextureForTarget(target, active_texture_))
                                ->sampler_parameters();
  switch (pname) {
    case GL_TEXTURE_MAG_FILTER: {
      *params = GLEnumFromMagFilter(active_sampler->mag_filter);
    } break;
    case GL_TEXTURE_MIN_FILTER: {
      *params = GLEnumFromMinFilter(active_sampler->min_filter);
    } break;
    case GL_TEXTURE_WRAP_S: {
      *params = GLEnumFromWrapMode(active_sampler->wrap_s);
    } break;
    case GL_TEXTURE_WRAP_T: {
      *params = GLEnumFromWrapMode(active_sampler->wrap_t);
    } break;

    default: {
      SetError(GL_INVALID_ENUM);
      return;
    }
  }
}

void Context::TexParameteri(GLenum target, GLenum pname, GLint param) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  Sampler* active_sampler = (*GetBoundTextureForTarget(target, active_texture_))
                                ->sampler_parameters();

  switch (pname) {
    case GL_TEXTURE_MAG_FILTER: {
      Sampler::MagFilter mag_filter = MagFilterFromGLEnum(param);
      if (mag_filter == Sampler::kMagFilterInvalid) {
        SetError(GL_INVALID_ENUM);
        return;
      }
      active_sampler->mag_filter = mag_filter;
    } break;
    case GL_TEXTURE_MIN_FILTER: {
      Sampler::MinFilter min_filter = MinFilterFromGLEnum(param);
      if (min_filter == Sampler::kMinFilterInvalid) {
        SetError(GL_INVALID_ENUM);
        return;
      }
      active_sampler->min_filter = min_filter;
    } break;
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T: {
      Sampler::WrapMode wrap_mode = WrapModeFromGLEnum(param);
      if (wrap_mode == Sampler::kWrapModeInvalid) {
        SetError(GL_INVALID_ENUM);
        return;
      }

      if (pname == GL_TEXTURE_WRAP_S) {
        active_sampler->wrap_s = wrap_mode;
      } else {
        SB_DCHECK(pname == GL_TEXTURE_WRAP_T);
        active_sampler->wrap_t = wrap_mode;
      }
    } break;

    default: {
      SetError(GL_INVALID_ENUM);
      return;
    }
  }

  enabled_textures_dirty_ = true;
}

namespace {

bool TextureFormatIsValid(GLenum format) {
  switch (format) {
    case GL_ALPHA:
    case GL_RGB:
    case GL_RGBA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
      return true;
    default:
      return false;
  }
}

bool TextureTypeIsValid(GLenum type) {
  switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
      return true;
    default:
      return false;
  }
}

// Converts a GL type and format to a glimp PixelFormat.  Information about
// the different possible values for type and format can be found here:
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
// Note that glimp may not support all possible formats described above.
PixelFormat PixelFormatFromGLTypeAndFormat(GLenum format, GLenum type) {
  if (type == GL_UNSIGNED_BYTE) {
    switch (format) {
      case GL_RGBA:
        return kPixelFormatRGBA8;
      case GL_ALPHA:
        return kPixelFormatA8;
      case GL_LUMINANCE_ALPHA:
        return kPixelFormatBA8;
    }
  }
  return kPixelFormatInvalid;
}

}  // namespace

void Context::TexImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid* pixels) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_TEXTURE_2D) {
    SB_NOTREACHED() << "Only target=GL_TEXTURE_2D is supported in glimp.";
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (width < 0 || height < 0 || level < 0 || border != 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  int max_texture_size = impl_->GetMaxTextureSize();
  if (width > max_texture_size || height > max_texture_size) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (format != internalformat) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  if (!TextureFormatIsValid(format)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (!TextureTypeIsValid(type)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  // Fold format and type together to determine a single glimp PixelFormat
  // value for the incoming data.
  PixelFormat pixel_format = PixelFormatFromGLTypeAndFormat(format, type);
  SB_DCHECK(pixel_format != kPixelFormatInvalid)
      << "Pixel format not supported by glimp.";

  nb::scoped_refptr<Texture> texture_object =
      *GetBoundTextureForTarget(target, active_texture_);
  if (!texture_object) {
    // According to the specification, no error is generated if no texture
    // is bound.
    //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
    return;
  }

  // The incoming pixel data should be aligned as the client has specified
  // that it will be.
  SB_DCHECK(nb::IsAligned(nb::AsInteger(pixels),
                          static_cast<uintptr_t>(unpack_alignment_)));

  // Determine pitch taking into account glPixelStorei() settings.
  int pitch_in_bytes = GetPitchForTextureData(width, pixel_format);

  texture_object->Initialize(level, pixel_format, width, height);

  if (bound_pixel_unpack_buffer_) {
    if (bound_pixel_unpack_buffer_->is_mapped() ||
        height * pitch_in_bytes > bound_pixel_unpack_buffer_->size_in_bytes()) {
      SetError(GL_INVALID_OPERATION);
      return;
    }

    texture_object->UpdateDataFromBuffer(
        level, 0, 0, width, height, pitch_in_bytes, bound_pixel_unpack_buffer_,
        nb::AsInteger(pixels));
  } else if (pixels) {
    texture_object->UpdateData(level, 0, 0, width, height, pitch_in_bytes,
                               pixels);
  }
}

void Context::TexSubImage2D(GLenum target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            const GLvoid* pixels) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_TEXTURE_2D) {
    SB_NOTREACHED() << "Only target=GL_TEXTURE_2D is supported in glimp.";
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (width < 0 || height < 0 || level < 0 || xoffset < 0 || yoffset < 0) {
    SetError(GL_INVALID_VALUE);
  }

  if (!TextureFormatIsValid(format)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (!TextureTypeIsValid(type)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  PixelFormat pixel_format = PixelFormatFromGLTypeAndFormat(format, type);
  SB_DCHECK(pixel_format != kPixelFormatInvalid)
      << "Pixel format not supported by glimp.";

  nb::scoped_refptr<Texture> texture_object =
      *GetBoundTextureForTarget(target, active_texture_);
  if (!texture_object) {
    // According to the specification, no error is generated if no texture
    // is bound.
    //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexSubImage2D.xml
    return;
  }

  if (!texture_object->texture_allocated() ||
      pixel_format != texture_object->pixel_format()) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  if (xoffset + width > texture_object->width() ||
      yoffset + height > texture_object->height()) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  // The incoming pixel data should be aligned as the client has specified
  // that it will be.
  SB_DCHECK(nb::IsAligned(nb::AsInteger(pixels),
                          static_cast<uintptr_t>(unpack_alignment_)));

  // Determine pitch taking into account glPixelStorei() settings.
  int pitch_in_bytes = GetPitchForTextureData(width, pixel_format);

  if (bound_pixel_unpack_buffer_) {
    if (bound_pixel_unpack_buffer_->is_mapped() ||
        height * pitch_in_bytes > bound_pixel_unpack_buffer_->size_in_bytes()) {
      SetError(GL_INVALID_OPERATION);
      return;
    }

    texture_object->UpdateDataFromBuffer(
        level, xoffset, yoffset, width, height, pitch_in_bytes,
        bound_pixel_unpack_buffer_, nb::AsInteger(pixels));
  } else {
    texture_object->UpdateData(level, xoffset, yoffset, width, height,
                               pitch_in_bytes, pixels);
  }
}

void Context::GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    nb::scoped_refptr<Framebuffer> framebuffer(new Framebuffer());

    framebuffers[i] = resource_manager_->RegisterFramebuffer(framebuffer);
  }
}

void Context::DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    if (framebuffers[i] == 0) {
      // Silently ignore 0 framebuffers.
      continue;
    }

    nb::scoped_refptr<Framebuffer> framebuffer_object =
        resource_manager_->DeregisterFramebuffer(framebuffers[i]);

    if (!framebuffer_object) {
      // The specification does not indicate that any error should be set
      // in the case that there was an error deleting a specific framebuffer.
      //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDeleteFramebuffers.xml
      return;
    }

    // If a bound framebuffer is deleted, set the bound framebuffer back to
    // the default framebuffer.
    if (framebuffer_object == draw_state_.framebuffer) {
      SetBoundDrawFramebufferToDefault();
    }
    if (framebuffer_object == read_framebuffer_) {
      SetBoundReadFramebufferToDefault();
    }
  }
}

void Context::BindFramebuffer(GLenum target, GLuint framebuffer) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_FRAMEBUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (framebuffer == 0) {
    SetBoundDrawFramebufferToDefault();
    SetBoundReadFramebufferToDefault();
    return;
  }

  nb::scoped_refptr<Framebuffer> framebuffer_object =
      resource_manager_->GetFramebuffer(framebuffer);

  if (!framebuffer_object) {
    // According to the specification, no error is generated if the buffer is
    // invalid.
    //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBindFramebuffer.xml
    SB_DLOG(WARNING) << "Could not glBindFramebuffer() to invalid framebuffer.";
    return;
  }

  draw_state_.framebuffer = framebuffer_object;
  draw_state_dirty_flags_.framebuffer_dirty = true;

  read_framebuffer_ = framebuffer_object;
}

void Context::FramebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   GLuint texture,
                                   GLint level) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_FRAMEBUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (IsDefaultDrawFramebufferBound() || IsDefaultReadFramebufferBound()) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  if (textarget != GL_TEXTURE_2D) {
    SB_NOTREACHED() << "Only textarget=GL_TEXTURE_2D is supported in glimp.";
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (attachment != GL_COLOR_ATTACHMENT0) {
    SB_NOTREACHED()
        << "Only attachment=GL_COLOR_ATTACHMENT0 is supported in glimp.";
    SetError(GL_INVALID_ENUM);
    return;
  }

  nb::scoped_refptr<Texture> texture_object;
  if (texture != 0) {
    texture_object = resource_manager_->GetTexture(texture);
    if (!texture_object) {
      SetError(GL_INVALID_OPERATION);
      return;
    }
  }

  draw_state_.framebuffer->AttachTexture2D(texture_object, level);
}

GLenum Context::CheckFramebufferStatus(GLenum target) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_FRAMEBUFFER) {
    SetError(GL_INVALID_ENUM);
    return 0;
  }

  return draw_state_.framebuffer->CheckFramebufferStatus();
}

void Context::FramebufferRenderbuffer(GLenum target,
                                      GLenum attachment,
                                      GLenum renderbuffertarget,
                                      GLuint renderbuffer) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_FRAMEBUFFER || renderbuffertarget != GL_RENDERBUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  SB_DCHECK(attachment != GL_COLOR_ATTACHMENT0)
      << "glimp does not support attaching color renderbuffers to "
         "framebuffers.";

  if (IsDefaultDrawFramebufferBound()) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  nb::scoped_refptr<Renderbuffer> renderbuffer_object = NULL;

  // Resolve the actual render buffer object to bind if we are not binding
  // render buffer 0, in which case we leave the value to set as NULL.
  if (renderbuffer != 0) {
    renderbuffer_object = resource_manager_->GetRenderbuffer(renderbuffer);

    if (!renderbuffer_object) {
      SetError(GL_INVALID_OPERATION);
      return;
    }
  }

  switch (attachment) {
    case GL_DEPTH_ATTACHMENT:
      draw_state_.framebuffer->SetDepthAttachment(renderbuffer_object);
      break;
    case GL_STENCIL_ATTACHMENT:
      draw_state_.framebuffer->SetStencilAttachment(renderbuffer_object);
      break;
    default:
      SetError(GL_INVALID_ENUM);
  }
}

void Context::GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    nb::scoped_refptr<Renderbuffer> renderbuffer(new Renderbuffer());

    renderbuffers[i] = resource_manager_->RegisterRenderbuffer(renderbuffer);
  }
}

void Context::DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (n < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  for (GLsizei i = 0; i < n; ++i) {
    if (renderbuffers[i] == 0) {
      // Silently ignore 0 renderbuffers.
      continue;
    }

    nb::scoped_refptr<Renderbuffer> renderbuffer_object =
        resource_manager_->DeregisterRenderbuffer(renderbuffers[i]);

    if (!renderbuffer_object) {
      // The specification does not indicate that any error should be set
      // in the case that there was an error deleting a specific renderbuffer.
      //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glDeleteRenderbuffers.xml
      return;
    }

    // If we're deleting the currently bound renderbuffer, set the currently
    // bound render buffer to NULL.
    if (renderbuffer_object == bound_renderbuffer_) {
      bound_renderbuffer_ = NULL;
    }
  }
}

void Context::BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_RENDERBUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (renderbuffer == 0) {
    bound_renderbuffer_ = NULL;
    return;
  }

  nb::scoped_refptr<Renderbuffer> renderbuffer_object =
      resource_manager_->GetRenderbuffer(renderbuffer);

  if (!renderbuffer_object) {
    // According to the specification, no error is generated if the buffer is
    // invalid.
    //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBindRenderbuffer.xml
    SB_DLOG(WARNING)
        << "Could not glBindRenderbuffer() to invalid renderbuffer.";
    return;
  }

  bound_renderbuffer_ = renderbuffer_object;
}

namespace {
// Valid formats as listed here:
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glRenderbufferStorage.xml
bool RenderbufferStorageFormatIsValid(GLenum internalformat) {
  switch (internalformat) {
    case GL_RGBA4:
    case GL_RGB565:
    case GL_RGB5_A1:
    case GL_DEPTH_COMPONENT16:
    case GL_STENCIL_INDEX8:
      return true;
    default:
      return false;
  }
}
}  // namespace

void Context::RenderbufferStorage(GLenum target,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (target != GL_RENDERBUFFER) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (!RenderbufferStorageFormatIsValid(internalformat)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  if (width < 0 || height < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (bound_renderbuffer_ == 0) {
    SetError(GL_INVALID_OPERATION);
    return;
  }

  bound_renderbuffer_->Initialize(internalformat, width, height);
}

void Context::StencilMask(GLuint mask) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (mask != 0xFFFFFFFF) {
    // If we are not setting stencil mask to its initial value then indicate
    // that our implementation is lacking.
    SB_NOTIMPLEMENTED();
  }
}

void Context::ClearStencil(GLint s) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (s != 0) {
    // If we are not setting stencil clear to its initial value then indicate
    // that our implementation is lacking.
    SB_NOTIMPLEMENTED();
  }
}

void Context::Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  draw_state_.viewport.rect = nb::Rect<int>(x, y, width, height);
  draw_state_dirty_flags_.viewport_dirty = true;
}

void Context::Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (x < 0) {
    SB_DLOG(WARNING) << "glScissor() x coordinate is set to negative.";
  }
  if (y < 0) {
    SB_DLOG(WARNING) << "glScissor() y coordinate is set to negative.";
  }
  draw_state_.scissor.rect = nb::Rect<int>(x, y, width, height);
  draw_state_dirty_flags_.scissor_dirty = true;
}

namespace {
// Converts from the GLenum passed into glVertexAttribPointer() to the enum
// defined in VertexAttribute.
static VertexAttributeType VertexAttributeTypeFromGLEnum(GLenum type) {
  switch (type) {
    case GL_BYTE:
      return kVertexAttributeTypeByte;
    case GL_UNSIGNED_BYTE:
      return kVertexAttributeTypeUnsignedByte;
    case GL_SHORT:
      return kVertexAttributeTypeShort;
    case GL_UNSIGNED_SHORT:
      return kVertexAttributeTypeUnsignedShort;
    case GL_FIXED:
      return kVertexAttributeTypeFixed;
    case GL_FLOAT:
      return kVertexAttributeTypeFloat;
    case GL_HALF_FLOAT:
      return kVertexAttributeTypeHalfFloat;
    default:
      return kVertexAttributeTypeInvalid;
  }
}
}  // namespace

void Context::VertexAttribPointer(GLuint indx,
                                  GLint size,
                                  GLenum type,
                                  GLboolean normalized,
                                  GLsizei stride,
                                  const GLvoid* ptr) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (indx >= GL_MAX_VERTEX_ATTRIBS) {
    SetError(GL_INVALID_VALUE);
    return;
  }
  if (size < 1 || size > 4) {
    SetError(GL_INVALID_VALUE);
    return;
  }
  if (stride < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  VertexAttributeType vertex_attribute_type =
      VertexAttributeTypeFromGLEnum(type);
  if (vertex_attribute_type == kVertexAttributeTypeInvalid) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  vertex_attrib_map_[indx] =
      VertexAttributeArray(size, vertex_attribute_type, normalized, stride,
                           static_cast<int>(reinterpret_cast<uintptr_t>(ptr)));
  if (enabled_vertex_attribs_.find(indx) != enabled_vertex_attribs_.end()) {
    enabled_vertex_attribs_dirty_ = true;
  }
}

void Context::EnableVertexAttribArray(GLuint index) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (index >= GL_MAX_VERTEX_ATTRIBS) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  enabled_vertex_attribs_.insert(index);
  enabled_vertex_attribs_dirty_ = true;
}

void Context::DisableVertexAttribArray(GLuint index) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (index >= GL_MAX_VERTEX_ATTRIBS) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  enabled_vertex_attribs_.erase(index);
  enabled_vertex_attribs_dirty_ = true;
}

void Context::VertexAttribfv(GLuint indx,
                             int elem_size,
                             const GLfloat* values) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(elem_size > 0);
  SB_DCHECK(elem_size <= 4);

  VertexAttributeConstant* value = &const_vertex_attrib_map_[indx];
  SbMemorySet(value, 0, sizeof(*value));
  for (int i = 0; i < elem_size; ++i) {
    value->data[i] = values[i];
  }
  value->size = elem_size;
  value->type = kVertexAttributeTypeFloat;

  enabled_vertex_attribs_dirty_ = true;
}

GLint Context::GetUniformLocation(GLuint program, const GLchar* name) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (name[0] == 'g' && name[1] == 'l' && name[2] == '_') {
    // |name| is not allowed to begin with the reserved prefix, "gl_".
    return -1;
  }

  nb::scoped_refptr<Program> program_object =
      resource_manager_->GetProgram(program);
  if (!program_object) {
    SetError(GL_INVALID_VALUE);
    return -1;
  }

  if (!program_object->linked()) {
    SetError(GL_INVALID_OPERATION);
    return -1;
  }

  return program_object->GetUniformLocation(name);
}

void Context::Uniformiv(GLint location,
                        GLsizei count,
                        GLsizei elem_size,
                        const GLint* v) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(elem_size >= 1 && elem_size <= 4);

  if (count < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (!draw_state_.used_program) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  GLenum result =
      draw_state_.used_program->Uniformiv(location, count, elem_size, v);
  if (result == GL_NO_ERROR) {
    draw_state_dirty_flags_.uniforms_dirty.Mark(location);
  } else {
    SetError(result);
  }
}

void Context::Uniformfv(GLint location,
                        GLsizei count,
                        GLsizei elem_size,
                        const GLfloat* v) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(elem_size >= 1 && elem_size <= 4);

  if (count < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (!draw_state_.used_program) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  GLenum result =
      draw_state_.used_program->Uniformfv(location, count, elem_size, v);
  if (result == GL_NO_ERROR) {
    draw_state_dirty_flags_.uniforms_dirty.Mark(location);
  } else {
    SetError(result);
  }
}

void Context::UniformMatrixfv(GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              GLsizei dim_size,
                              const GLfloat* value) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(dim_size >= 2 && dim_size <= 4);

  if (transpose != GL_FALSE) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (!draw_state_.used_program) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  GLenum result = draw_state_.used_program->UniformMatrixfv(location, count,
                                                            dim_size, value);
  if (result == GL_NO_ERROR) {
    draw_state_dirty_flags_.uniforms_dirty.Mark(location);
  } else {
    SetError(result);
  }
}

namespace {
DrawMode DrawModeFromGLEnum(GLenum mode) {
  switch (mode) {
    case GL_POINTS:
      return kDrawModePoints;
    case GL_LINE_STRIP:
      return kDrawModeLineStrip;
    case GL_LINE_LOOP:
      return kDrawModeLineLoop;
    case GL_LINES:
      return kDrawModeLines;
    case GL_TRIANGLE_STRIP:
      return kDrawModeTriangleStrip;
    case GL_TRIANGLE_FAN:
      return kDrawModeTriangleFan;
    case GL_TRIANGLES:
      return kDrawModeTriangles;
    default:
      return kDrawModeInvalid;
  }
}
}  // namespace

void Context::DrawArrays(GLenum mode, GLint first, GLsizei count) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (count < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (draw_state_.framebuffer->CheckFramebufferStatus() !=
      GL_FRAMEBUFFER_COMPLETE) {
    SetError(GL_INVALID_FRAMEBUFFER_OPERATION);
    return;
  }

  DrawMode draw_mode = DrawModeFromGLEnum(mode);
  if (draw_mode == kDrawModeInvalid) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  SB_DCHECK(draw_state_.array_buffer)
      << "glimp only supports vertices from vertex buffers.";

  CompressDrawStateForDrawCall();

  impl_->DrawArrays(draw_mode, first, count, draw_state_,
                    &draw_state_dirty_flags_);
}

namespace {
IndexDataType IndexDataTypeFromGLenum(GLenum type) {
  switch (type) {
    case GL_UNSIGNED_BYTE:
      return kIndexDataTypeUnsignedByte;
    case GL_UNSIGNED_SHORT:
      return kIndexDataTypeUnsignedShort;
    default:
      return kIndexDataTypeInvalid;
  }
}
}  // namespace

void Context::DrawElements(GLenum mode,
                           GLsizei count,
                           GLenum type,
                           const GLvoid* indices) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (count < 0) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  if (draw_state_.framebuffer->CheckFramebufferStatus() !=
      GL_FRAMEBUFFER_COMPLETE) {
    SetError(GL_INVALID_FRAMEBUFFER_OPERATION);
    return;
  }

  DrawMode draw_mode = DrawModeFromGLEnum(mode);
  if (draw_mode == kDrawModeInvalid) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  IndexDataType index_data_type = IndexDataTypeFromGLenum(type);
  if (type == kIndexDataTypeInvalid) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  SB_DCHECK(draw_state_.array_buffer)
      << "glimp only supports vertices from vertex buffers.";
  SB_DCHECK(draw_state_.element_array_buffer)
      << "glimp only supports indices from element vertex buffers.";

  CompressDrawStateForDrawCall();

  impl_->DrawElements(draw_mode, count, index_data_type,
                      reinterpret_cast<intptr_t>(indices), draw_state_,
                      &draw_state_dirty_flags_);
}

namespace {
bool ValidReadPixelsFormat(GLenum format) {
  switch (format) {
    case GL_RGBA:
    case GL_RGBA_INTEGER:
      return true;
      break;
    default:
      return false;
      break;
  }
}

bool ValidReadPixelsType(GLenum type) {
  switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
      return true;
      break;
    default:
      return false;
      break;
  }
}
}  // namespace

void Context::ReadPixels(GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         GLvoid* pixels) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (!ValidReadPixelsFormat(format) || !ValidReadPixelsType(type)) {
    SetError(GL_INVALID_ENUM);
    return;
  }

  SB_DCHECK(format == GL_RGBA) << "glimp only supports format=GL_RGBA.";
  SB_DCHECK(type == GL_UNSIGNED_BYTE)
      << "glimp only supports type=GL_UNSIGNED_BYTE.";

  SB_DCHECK(read_framebuffer_->color_attachment_texture())
      << "glimp only supports glReadPixels() calls on non-default "
         "framebuffers.";

  if (x < 0 || y < 0 || width < 0 || height < 0 ||
      x + width > read_framebuffer_->GetWidth() ||
      y + height > read_framebuffer_->GetHeight()) {
    SetError(GL_INVALID_VALUE);
    return;
  }

  // Ensure that all GPU activity (in particular, texture writes) complete
  // before we attempt to read pixel data from the texture.
  Finish();

  read_framebuffer_->color_attachment_texture()->ReadPixelsAsRGBA8(
      x, y, width, height, width * BytesPerPixel(kPixelFormatRGBA8), pixels);
}

void Context::Flush() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  impl_->Flush();
}

void Context::Finish() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  impl_->Finish();
}

void Context::SwapBuffers() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  egl::Surface* surface = default_draw_framebuffer_->color_attachment_surface();
  // If surface is a pixel buffer or a pixmap, eglSwapBuffers has no effect, and
  // no error is generated.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglSwapBuffers.xhtml
  if (surface->impl()->IsWindowSurface()) {
    Flush();
    impl_->SwapBuffers(surface);
  }
}

bool Context::BindTextureToEGLSurface(egl::Surface* surface) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  SB_DCHECK(surface->GetTextureTarget() == EGL_TEXTURE_2D);

  const nb::scoped_refptr<Texture>& current_texture =
      *GetBoundTextureForTarget(GL_TEXTURE_2D, active_texture_);

  if (!current_texture) {
    SB_DLOG(WARNING) << "No texture is currently bound during call to "
                        "eglBindTexImage().";
    return false;
  }

  SB_DCHECK(bound_egl_surfaces_.find(surface) == bound_egl_surfaces_.end());

  bool success = current_texture->BindToEGLSurface(surface);
  if (success) {
    bound_egl_surfaces_[surface] = current_texture;
  }

  return success;
}

bool Context::ReleaseTextureFromEGLSurface(egl::Surface* surface) {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  std::map<egl::Surface*, nb::scoped_refptr<Texture> >::iterator found =
      bound_egl_surfaces_.find(surface);
  if (found == bound_egl_surfaces_.end()) {
    SB_LOG(WARNING) << "Releasing EGLSurface was never bound to a texture in "
                       "this context.";
    return false;
  }

  bool success = found->second->ReleaseFromEGLSurface(surface);
  if (success) {
    bound_egl_surfaces_.erase(found);
  }
  return success;
}

void Context::UpdateVertexAttribsInDrawState() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  // Setup the dense list of enabled vertex attributes.
  draw_state_.vertex_attributes.clear();
  for (std::set<unsigned int>::const_iterator iter =
           enabled_vertex_attribs_.begin();
       iter != enabled_vertex_attribs_.end(); ++iter) {
    draw_state_.vertex_attributes.push_back(
        std::make_pair(*iter, &vertex_attrib_map_[*iter]));
  }

  draw_state_.constant_vertex_attributes.clear();
  for (std::map<unsigned int, VertexAttributeConstant>::iterator iter =
           const_vertex_attrib_map_.begin();
       iter != const_vertex_attrib_map_.end(); ++iter) {
    // Add constant vertex attributes only if they do not have a vertex
    // attribute array enabled for them.
    if (enabled_vertex_attribs_.find(iter->first) ==
        enabled_vertex_attribs_.end()) {
      draw_state_.constant_vertex_attributes.push_back(
          std::make_pair(iter->first, &iter->second));
    }
  }

  draw_state_dirty_flags_.vertex_attributes_dirty = true;
  enabled_vertex_attribs_dirty_ = false;
}

void Context::UpdateSamplersInDrawState() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  // Setup the list of enabled samplers.
  draw_state_.textures.clear();
  int max_active_textures = impl_->GetMaxFragmentTextureUnits();
  for (int i = 0; i < max_active_textures; ++i) {
    if (texture_units_[i]) {
      draw_state_.textures.push_back(std::make_pair(
          static_cast<unsigned int>(i), texture_units_[i].get()));
    }
  }

  draw_state_dirty_flags_.textures_dirty = true;
  enabled_textures_dirty_ = false;
}

void Context::CompressDrawStateForDrawCall() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (enabled_vertex_attribs_dirty_) {
    UpdateVertexAttribsInDrawState();
    SB_DCHECK(enabled_vertex_attribs_dirty_ == false);
  }

  if (enabled_textures_dirty_) {
    UpdateSamplersInDrawState();
    SB_DCHECK(enabled_textures_dirty_ == false);
  }
}

void Context::MarkUsedProgramDirty() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  draw_state_dirty_flags_.used_program_dirty = true;
  // Switching programs marks all uniforms, samplers and vertex attributes
  // as being dirty as well, since they are all properties of the program.
  draw_state_dirty_flags_.vertex_attributes_dirty = true;
  draw_state_dirty_flags_.textures_dirty = true;
  draw_state_dirty_flags_.uniforms_dirty.MarkAll();
}

void Context::SetBoundDrawFramebufferToDefault() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (draw_state_.framebuffer != default_draw_framebuffer_) {
    draw_state_.framebuffer = default_draw_framebuffer_;
    draw_state_dirty_flags_.framebuffer_dirty = true;
  }
}

void Context::SetBoundReadFramebufferToDefault() {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  if (read_framebuffer_ != default_read_framebuffer_) {
    read_framebuffer_ = default_read_framebuffer_;
  }
}

bool Context::IsDefaultDrawFramebufferBound() const {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  return draw_state_.framebuffer == default_draw_framebuffer_;
}

bool Context::IsDefaultReadFramebufferBound() const {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  return read_framebuffer_ == default_read_framebuffer_;
}

int Context::GetPitchForTextureData(int width, PixelFormat pixel_format) const {
  GLIMP_TRACE_EVENT0(__FUNCTION__);
  // The equations for determining the pitch are described here:
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glPixelStorei.xhtml
  int n = BytesPerPixel(pixel_format);
  int s = 1;
  int len = unpack_row_length_ > 0 ? unpack_row_length_ : width;
  int a = unpack_alignment_;

  if (s >= a) {
    return n * len;
  } else {
    return nb::AlignUp(s * n * len, a) / s;
  }
}

}  // namespace gles
}  // namespace glimp
