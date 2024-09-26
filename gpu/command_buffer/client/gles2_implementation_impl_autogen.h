// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_implementation.cc to define the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_IMPL_AUTOGEN_H_

void GLES2Implementation::AttachShader(GLuint program, GLuint shader) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glAttachShader(" << program << ", "
                     << shader << ")");
  helper_->AttachShader(program, shader);
  CheckGLError();
}

void GLES2Implementation::BindBuffer(GLenum target, GLuint buffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindBuffer("
                     << GLES2Util::GetStringBufferTarget(target) << ", "
                     << buffer << ")");
  if (IsBufferReservedId(buffer)) {
    SetGLError(GL_INVALID_OPERATION, "BindBuffer", "buffer reserved id");
    return;
  }
  BindBufferHelper(target, buffer);
  CheckGLError();
}

void GLES2Implementation::BindBufferBase(GLenum target,
                                         GLuint index,
                                         GLuint buffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindBufferBase("
                     << GLES2Util::GetStringIndexedBufferTarget(target) << ", "
                     << index << ", " << buffer << ")");
  if (IsBufferReservedId(buffer)) {
    SetGLError(GL_INVALID_OPERATION, "BindBufferBase", "buffer reserved id");
    return;
  }
  BindBufferBaseHelper(target, index, buffer);
  CheckGLError();
}

void GLES2Implementation::BindBufferRange(GLenum target,
                                          GLuint index,
                                          GLuint buffer,
                                          GLintptr offset,
                                          GLsizeiptr size) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindBufferRange("
                     << GLES2Util::GetStringIndexedBufferTarget(target) << ", "
                     << index << ", " << buffer << ", " << offset << ", "
                     << size << ")");
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE, "glBindBufferRange", "offset < 0");
    return;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBindBufferRange", "size < 0");
    return;
  }
  if (IsBufferReservedId(buffer)) {
    SetGLError(GL_INVALID_OPERATION, "BindBufferRange", "buffer reserved id");
    return;
  }
  BindBufferRangeHelper(target, index, buffer, offset, size);
  CheckGLError();
}

void GLES2Implementation::BindFramebuffer(GLenum target, GLuint framebuffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindFramebuffer("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << framebuffer << ")");
  if (IsFramebufferReservedId(framebuffer)) {
    SetGLError(GL_INVALID_OPERATION, "BindFramebuffer",
               "framebuffer reserved id");
    return;
  }
  BindFramebufferHelper(target, framebuffer);
  CheckGLError();
}

void GLES2Implementation::BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindRenderbuffer("
                     << GLES2Util::GetStringRenderBufferTarget(target) << ", "
                     << renderbuffer << ")");
  if (IsRenderbufferReservedId(renderbuffer)) {
    SetGLError(GL_INVALID_OPERATION, "BindRenderbuffer",
               "renderbuffer reserved id");
    return;
  }
  BindRenderbufferHelper(target, renderbuffer);
  CheckGLError();
}

void GLES2Implementation::BindSampler(GLuint unit, GLuint sampler) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindSampler(" << unit << ", "
                     << sampler << ")");
  if (IsSamplerReservedId(sampler)) {
    SetGLError(GL_INVALID_OPERATION, "BindSampler", "sampler reserved id");
    return;
  }
  BindSamplerHelper(unit, sampler);
  CheckGLError();
}

void GLES2Implementation::BindTexture(GLenum target, GLuint texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindTexture("
                     << GLES2Util::GetStringTextureBindTarget(target) << ", "
                     << texture << ")");
  if (IsTextureReservedId(texture)) {
    SetGLError(GL_INVALID_OPERATION, "BindTexture", "texture reserved id");
    return;
  }
  BindTextureHelper(target, texture);
  CheckGLError();
}

void GLES2Implementation::BindTransformFeedback(GLenum target,
                                                GLuint transformfeedback) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindTransformFeedback("
                     << GLES2Util::GetStringTransformFeedbackBindTarget(target)
                     << ", " << transformfeedback << ")");
  if (IsTransformFeedbackReservedId(transformfeedback)) {
    SetGLError(GL_INVALID_OPERATION, "BindTransformFeedback",
               "transformfeedback reserved id");
    return;
  }
  BindTransformFeedbackHelper(target, transformfeedback);
  CheckGLError();
}

void GLES2Implementation::BlendColor(GLclampf red,
                                     GLclampf green,
                                     GLclampf blue,
                                     GLclampf alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendColor(" << red << ", "
                     << green << ", " << blue << ", " << alpha << ")");
  helper_->BlendColor(red, green, blue, alpha);
  CheckGLError();
}

void GLES2Implementation::BlendEquation(GLenum mode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendEquation("
                     << GLES2Util::GetStringEquation(mode) << ")");
  helper_->BlendEquation(mode);
  CheckGLError();
}

void GLES2Implementation::BlendEquationSeparate(GLenum modeRGB,
                                                GLenum modeAlpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendEquationSeparate("
                     << GLES2Util::GetStringEquation(modeRGB) << ", "
                     << GLES2Util::GetStringEquation(modeAlpha) << ")");
  helper_->BlendEquationSeparate(modeRGB, modeAlpha);
  CheckGLError();
}

void GLES2Implementation::BlendFunc(GLenum sfactor, GLenum dfactor) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendFunc("
                     << GLES2Util::GetStringSrcBlendFactor(sfactor) << ", "
                     << GLES2Util::GetStringDstBlendFactor(dfactor) << ")");
  helper_->BlendFunc(sfactor, dfactor);
  CheckGLError();
}

void GLES2Implementation::BlendFuncSeparate(GLenum srcRGB,
                                            GLenum dstRGB,
                                            GLenum srcAlpha,
                                            GLenum dstAlpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendFuncSeparate("
                     << GLES2Util::GetStringSrcBlendFactor(srcRGB) << ", "
                     << GLES2Util::GetStringDstBlendFactor(dstRGB) << ", "
                     << GLES2Util::GetStringSrcBlendFactor(srcAlpha) << ", "
                     << GLES2Util::GetStringDstBlendFactor(dstAlpha) << ")");
  helper_->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  CheckGLError();
}

GLenum GLES2Implementation::CheckFramebufferStatus(GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::CheckFramebufferStatus");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCheckFramebufferStatus("
                     << GLES2Util::GetStringFramebufferTarget(target) << ")");
  typedef cmds::CheckFramebufferStatus::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FRAMEBUFFER_UNSUPPORTED;
  }
  *result = 0;
  helper_->CheckFramebufferStatus(target, GetResultShmId(), result.offset());
  WaitForCmd();
  GLenum result_value = *result;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

void GLES2Implementation::Clear(GLbitfield mask) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClear(" << mask << ")");
  helper_->Clear(mask);
  CheckGLError();
}

void GLES2Implementation::ClearBufferfi(GLenum buffer,
                                        GLint drawbuffers,
                                        GLfloat depth,
                                        GLint stencil) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearBufferfi("
                     << GLES2Util::GetStringBufferfi(buffer) << ", "
                     << drawbuffers << ", " << depth << ", " << stencil << ")");
  helper_->ClearBufferfi(buffer, drawbuffers, depth, stencil);
  CheckGLError();
}

void GLES2Implementation::ClearBufferfv(GLenum buffer,
                                        GLint drawbuffers,
                                        const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearBufferfv("
                     << GLES2Util::GetStringBufferfv(buffer) << ", "
                     << drawbuffers << ", " << static_cast<const void*>(value)
                     << ")");
  uint32_t count = GLES2Util::CalcClearBufferfvDataCount(buffer);
  DCHECK_LE(count, 4u);
  if (count == 0) {
    SetGLErrorInvalidEnum("glClearBufferfv", buffer, "buffer");
    return;
  }
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << value[ii]);
  helper_->ClearBufferfvImmediate(buffer, drawbuffers, value);
  CheckGLError();
}

void GLES2Implementation::ClearBufferiv(GLenum buffer,
                                        GLint drawbuffers,
                                        const GLint* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearBufferiv("
                     << GLES2Util::GetStringBufferiv(buffer) << ", "
                     << drawbuffers << ", " << static_cast<const void*>(value)
                     << ")");
  uint32_t count = GLES2Util::CalcClearBufferivDataCount(buffer);
  DCHECK_LE(count, 4u);
  if (count == 0) {
    SetGLErrorInvalidEnum("glClearBufferiv", buffer, "buffer");
    return;
  }
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << value[ii]);
  helper_->ClearBufferivImmediate(buffer, drawbuffers, value);
  CheckGLError();
}

void GLES2Implementation::ClearBufferuiv(GLenum buffer,
                                         GLint drawbuffers,
                                         const GLuint* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearBufferuiv("
                     << GLES2Util::GetStringBufferuiv(buffer) << ", "
                     << drawbuffers << ", " << static_cast<const void*>(value)
                     << ")");
  uint32_t count = GLES2Util::CalcClearBufferuivDataCount(buffer);
  DCHECK_LE(count, 4u);
  if (count == 0) {
    SetGLErrorInvalidEnum("glClearBufferuiv", buffer, "buffer");
    return;
  }
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << value[ii]);
  helper_->ClearBufferuivImmediate(buffer, drawbuffers, value);
  CheckGLError();
}

void GLES2Implementation::ClearColor(GLclampf red,
                                     GLclampf green,
                                     GLclampf blue,
                                     GLclampf alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearColor(" << red << ", "
                     << green << ", " << blue << ", " << alpha << ")");
  helper_->ClearColor(red, green, blue, alpha);
  CheckGLError();
}

void GLES2Implementation::ClearDepthf(GLclampf depth) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearDepthf(" << depth << ")");
  helper_->ClearDepthf(depth);
  CheckGLError();
}

void GLES2Implementation::ClearStencil(GLint s) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClearStencil(" << s << ")");
  helper_->ClearStencil(s);
  CheckGLError();
}

void GLES2Implementation::ColorMask(GLboolean red,
                                    GLboolean green,
                                    GLboolean blue,
                                    GLboolean alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glColorMask("
                     << GLES2Util::GetStringBool(red) << ", "
                     << GLES2Util::GetStringBool(green) << ", "
                     << GLES2Util::GetStringBool(blue) << ", "
                     << GLES2Util::GetStringBool(alpha) << ")");
  helper_->ColorMask(red, green, blue, alpha);
  CheckGLError();
}

void GLES2Implementation::CompileShader(GLuint shader) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCompileShader(" << shader
                     << ")");
  helper_->CompileShader(shader);
  CheckGLError();
}

void GLES2Implementation::CopyTexImage2D(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCopyTexImage2D("
          << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", "
          << GLES2Util::GetStringTextureInternalFormat(internalformat) << ", "
          << x << ", " << y << ", " << width << ", " << height << ", " << border
          << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D", "height < 0");
    return;
  }
  if (border != 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D", "border GL_INVALID_VALUE");
    return;
  }
  helper_->CopyTexImage2D(target, level, internalformat, x, y, width, height);
  CheckGLError();
}

void GLES2Implementation::CopyTexSubImage2D(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCopyTexSubImage2D("
                     << GLES2Util::GetStringTextureTarget(target) << ", "
                     << level << ", " << xoffset << ", " << yoffset << ", " << x
                     << ", " << y << ", " << width << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D", "height < 0");
    return;
  }
  helper_->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width,
                             height);
  CheckGLError();
}

void GLES2Implementation::CopyTexSubImage3D(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCopyTexSubImage3D("
                     << GLES2Util::GetStringTexture3DTarget(target) << ", "
                     << level << ", " << xoffset << ", " << yoffset << ", "
                     << zoffset << ", " << x << ", " << y << ", " << width
                     << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage3D", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage3D", "height < 0");
    return;
  }
  helper_->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y,
                             width, height);
  CheckGLError();
}

GLuint GLES2Implementation::CreateProgram() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCreateProgram("
                     << ")");
  GLuint client_id;
  GetIdHandler(SharedIdNamespaces::kProgramsAndShaders)
      ->MakeIds(this, 0, 1, &client_id);
  helper_->CreateProgram(client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  CheckGLError();
  return client_id;
}

GLuint GLES2Implementation::CreateShader(GLenum type) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCreateShader("
                     << GLES2Util::GetStringShaderType(type) << ")");
  GLuint client_id;
  GetIdHandler(SharedIdNamespaces::kProgramsAndShaders)
      ->MakeIds(this, 0, 1, &client_id);
  helper_->CreateShader(type, client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  CheckGLError();
  return client_id;
}

void GLES2Implementation::CullFace(GLenum mode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCullFace("
                     << GLES2Util::GetStringFaceType(mode) << ")");
  helper_->CullFace(mode);
  CheckGLError();
}

void GLES2Implementation::DeleteBuffers(GLsizei n, const GLuint* buffers) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteBuffers(" << n << ", "
                     << static_cast<const void*>(buffers) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << buffers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(buffers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteBuffers", "n < 0");
    return;
  }
  DeleteBuffersHelper(n, buffers);
  CheckGLError();
}

void GLES2Implementation::DeleteFramebuffers(GLsizei n,
                                             const GLuint* framebuffers) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteFramebuffers(" << n << ", "
                     << static_cast<const void*>(framebuffers) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << framebuffers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(framebuffers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteFramebuffers", "n < 0");
    return;
  }
  DeleteFramebuffersHelper(n, framebuffers);
  CheckGLError();
}

void GLES2Implementation::DeleteProgram(GLuint program) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteProgram(" << program
                     << ")");
  if (program == 0)
    return;
  DeleteProgramHelper(program);
  CheckGLError();
}

void GLES2Implementation::DeleteRenderbuffers(GLsizei n,
                                              const GLuint* renderbuffers) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteRenderbuffers(" << n
                     << ", " << static_cast<const void*>(renderbuffers) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << renderbuffers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(renderbuffers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteRenderbuffers", "n < 0");
    return;
  }
  DeleteRenderbuffersHelper(n, renderbuffers);
  CheckGLError();
}

void GLES2Implementation::DeleteSamplers(GLsizei n, const GLuint* samplers) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteSamplers(" << n << ", "
                     << static_cast<const void*>(samplers) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << samplers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(samplers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteSamplers", "n < 0");
    return;
  }
  DeleteSamplersHelper(n, samplers);
  CheckGLError();
}

void GLES2Implementation::DeleteSync(GLsync sync) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteSync(" << sync << ")");
  if (sync == 0)
    return;
  DeleteSyncHelper(sync);
  CheckGLError();
}

void GLES2Implementation::DeleteShader(GLuint shader) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteShader(" << shader << ")");
  if (shader == 0)
    return;
  DeleteShaderHelper(shader);
  CheckGLError();
}

void GLES2Implementation::DeleteTextures(GLsizei n, const GLuint* textures) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteTextures(" << n << ", "
                     << static_cast<const void*>(textures) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << textures[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(textures[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteTextures", "n < 0");
    return;
  }
  DeleteTexturesHelper(n, textures);
  CheckGLError();
}

void GLES2Implementation::DeleteTransformFeedbacks(GLsizei n,
                                                   const GLuint* ids) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteTransformFeedbacks(" << n
                     << ", " << static_cast<const void*>(ids) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << ids[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(ids[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteTransformFeedbacks", "n < 0");
    return;
  }
  DeleteTransformFeedbacksHelper(n, ids);
  CheckGLError();
}

void GLES2Implementation::DepthFunc(GLenum func) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDepthFunc("
                     << GLES2Util::GetStringCmpFunction(func) << ")");
  helper_->DepthFunc(func);
  CheckGLError();
}

void GLES2Implementation::DepthMask(GLboolean flag) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDepthMask("
                     << GLES2Util::GetStringBool(flag) << ")");
  helper_->DepthMask(flag);
  CheckGLError();
}

void GLES2Implementation::DepthRangef(GLclampf zNear, GLclampf zFar) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDepthRangef(" << zNear << ", "
                     << zFar << ")");
  helper_->DepthRangef(zNear, zFar);
  CheckGLError();
}

void GLES2Implementation::DetachShader(GLuint program, GLuint shader) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDetachShader(" << program << ", "
                     << shader << ")");
  helper_->DetachShader(program, shader);
  CheckGLError();
}

GLsync GLES2Implementation::FenceSync(GLenum condition, GLbitfield flags) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFenceSync("
                     << GLES2Util::GetStringSyncCondition(condition) << ", "
                     << flags << ")");
  if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE) {
    SetGLError(GL_INVALID_ENUM, "glFenceSync", "condition GL_INVALID_ENUM");
    return 0;
  }
  if (flags != 0) {
    SetGLError(GL_INVALID_VALUE, "glFenceSync", "flags GL_INVALID_VALUE");
    return 0;
  }
  GLuint client_id;
  GetIdHandler(SharedIdNamespaces::kSyncs)->MakeIds(this, 0, 1, &client_id);
  helper_->FenceSync(client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  CheckGLError();
  return reinterpret_cast<GLsync>(client_id);
}

void GLES2Implementation::FramebufferRenderbuffer(GLenum target,
                                                  GLenum attachment,
                                                  GLenum renderbuffertarget,
                                                  GLuint renderbuffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glFramebufferRenderbuffer("
          << GLES2Util::GetStringFramebufferTarget(target) << ", "
          << GLES2Util::GetStringAttachment(attachment) << ", "
          << GLES2Util::GetStringRenderBufferTarget(renderbuffertarget) << ", "
          << renderbuffer << ")");
  helper_->FramebufferRenderbuffer(target, attachment, renderbuffertarget,
                                   renderbuffer);
  CheckGLError();
}

void GLES2Implementation::FramebufferTexture2D(GLenum target,
                                               GLenum attachment,
                                               GLenum textarget,
                                               GLuint texture,
                                               GLint level) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFramebufferTexture2D("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << GLES2Util::GetStringAttachment(attachment) << ", "
                     << GLES2Util::GetStringTextureFboTarget(textarget) << ", "
                     << texture << ", " << level << ")");
  helper_->FramebufferTexture2D(target, attachment, textarget, texture, level);
  CheckGLError();
}

void GLES2Implementation::FramebufferTextureLayer(GLenum target,
                                                  GLenum attachment,
                                                  GLuint texture,
                                                  GLint level,
                                                  GLint layer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFramebufferTextureLayer("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << GLES2Util::GetStringAttachment(attachment) << ", "
                     << texture << ", " << level << ", " << layer << ")");
  helper_->FramebufferTextureLayer(target, attachment, texture, level, layer);
  CheckGLError();
}

void GLES2Implementation::FrontFace(GLenum mode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFrontFace("
                     << GLES2Util::GetStringFaceMode(mode) << ")");
  helper_->FrontFace(mode);
  CheckGLError();
}

void GLES2Implementation::GenBuffers(GLsizei n, GLuint* buffers) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenBuffers(" << n << ", "
                     << static_cast<const void*>(buffers) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenBuffers", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GetIdHandler(SharedIdNamespaces::kBuffers)->MakeIds(this, 0, n, buffers);
  GenBuffersHelper(n, buffers);
  helper_->GenBuffersImmediate(n, buffers);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::Flush();
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << buffers[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::GenerateMipmap(GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenerateMipmap("
                     << GLES2Util::GetStringTextureBindTarget(target) << ")");
  helper_->GenerateMipmap(target);
  CheckGLError();
}

void GLES2Implementation::GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenFramebuffers(" << n << ", "
                     << static_cast<const void*>(framebuffers) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenFramebuffers", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kFramebuffers);
  for (GLsizei ii = 0; ii < n; ++ii)
    framebuffers[ii] = id_allocator->AllocateID();
  GenFramebuffersHelper(n, framebuffers);
  helper_->GenFramebuffersImmediate(n, framebuffers);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << framebuffers[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenRenderbuffers(" << n << ", "
                     << static_cast<const void*>(renderbuffers) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenRenderbuffers", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GetIdHandler(SharedIdNamespaces::kRenderbuffers)
      ->MakeIds(this, 0, n, renderbuffers);
  GenRenderbuffersHelper(n, renderbuffers);
  helper_->GenRenderbuffersImmediate(n, renderbuffers);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::Flush();
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << renderbuffers[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::GenSamplers(GLsizei n, GLuint* samplers) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenSamplers(" << n << ", "
                     << static_cast<const void*>(samplers) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenSamplers", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GetIdHandler(SharedIdNamespaces::kSamplers)->MakeIds(this, 0, n, samplers);
  GenSamplersHelper(n, samplers);
  helper_->GenSamplersImmediate(n, samplers);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::Flush();
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << samplers[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::GenTextures(GLsizei n, GLuint* textures) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenTextures(" << n << ", "
                     << static_cast<const void*>(textures) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenTextures", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GetIdHandler(SharedIdNamespaces::kTextures)->MakeIds(this, 0, n, textures);
  GenTexturesHelper(n, textures);
  helper_->GenTexturesImmediate(n, textures);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::Flush();
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << textures[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::GenTransformFeedbacks(GLsizei n, GLuint* ids) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenTransformFeedbacks(" << n
                     << ", " << static_cast<const void*>(ids) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenTransformFeedbacks", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kTransformFeedbacks);
  for (GLsizei ii = 0; ii < n; ++ii)
    ids[ii] = id_allocator->AllocateID();
  GenTransformFeedbacksHelper(n, ids);
  helper_->GenTransformFeedbacksImmediate(n, ids);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << ids[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::GetBooleanv(GLenum pname, GLboolean* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLboolean, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetBooleanv("
                     << GLES2Util::GetStringGLState(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetBooleanv");
  if (GetBooleanvHelper(pname, params)) {
    return;
  }
  typedef cmds::GetBooleanv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetBooleanv(pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetBooleani_v(GLenum pname,
                                        GLuint index,
                                        GLboolean* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLboolean, data);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetBooleani_v("
                     << GLES2Util::GetStringIndexedGLState(pname) << ", "
                     << index << ", " << static_cast<const void*>(data) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetBooleani_v");
  if (GetBooleani_vHelper(pname, index, data)) {
    return;
  }
  typedef cmds::GetBooleani_v::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetBooleani_v(pname, index, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(data);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetBufferParameteri64v(GLenum target,
                                                 GLenum pname,
                                                 GLint64* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetBufferParameteri64v("
                     << GLES2Util::GetStringBufferTarget(target) << ", "
                     << GLES2Util::GetStringBufferParameter64(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetBufferParameteri64v");
  if (GetBufferParameteri64vHelper(target, pname, params)) {
    return;
  }
  typedef cmds::GetBufferParameteri64v::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetBufferParameteri64v(target, pname, GetResultShmId(),
                                  result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetBufferParameteriv(GLenum target,
                                               GLenum pname,
                                               GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetBufferParameteriv("
                     << GLES2Util::GetStringBufferTarget(target) << ", "
                     << GLES2Util::GetStringBufferParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetBufferParameteriv");
  if (GetBufferParameterivHelper(target, pname, params)) {
    return;
  }
  typedef cmds::GetBufferParameteriv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetBufferParameteriv(target, pname, GetResultShmId(),
                                result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetFloatv(GLenum pname, GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetFloatv("
                     << GLES2Util::GetStringGLState(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetFloatv");
  if (GetFloatvHelper(pname, params)) {
    return;
  }
  typedef cmds::GetFloatv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetFloatv(pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetFramebufferAttachmentParameteriv(GLenum target,
                                                              GLenum attachment,
                                                              GLenum pname,
                                                              GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glGetFramebufferAttachmentParameteriv("
          << GLES2Util::GetStringFramebufferTarget(target) << ", "
          << GLES2Util::GetStringAttachmentQuery(attachment) << ", "
          << GLES2Util::GetStringFramebufferAttachmentParameter(pname) << ", "
          << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu",
               "GLES2Implementation::GetFramebufferAttachmentParameteriv");
  if (GetFramebufferAttachmentParameterivHelper(target, attachment, pname,
                                                params)) {
    return;
  }
  typedef cmds::GetFramebufferAttachmentParameteriv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetInteger64v(GLenum pname, GLint64* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetInteger64v("
                     << GLES2Util::GetStringGLState(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetInteger64v");
  if (GetInteger64vHelper(pname, params)) {
    return;
  }
  typedef cmds::GetInteger64v::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetInteger64v(pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetIntegeri_v(GLenum pname,
                                        GLuint index,
                                        GLint* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, data);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetIntegeri_v("
                     << GLES2Util::GetStringIndexedGLState(pname) << ", "
                     << index << ", " << static_cast<const void*>(data) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetIntegeri_v");
  if (GetIntegeri_vHelper(pname, index, data)) {
    return;
  }
  typedef cmds::GetIntegeri_v::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetIntegeri_v(pname, index, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(data);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetInteger64i_v(GLenum pname,
                                          GLuint index,
                                          GLint64* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetInteger64i_v("
                     << GLES2Util::GetStringIndexedGLState(pname) << ", "
                     << index << ", " << static_cast<const void*>(data) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetInteger64i_v");
  if (GetInteger64i_vHelper(pname, index, data)) {
    return;
  }
  typedef cmds::GetInteger64i_v::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetInteger64i_v(pname, index, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(data);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetIntegerv(GLenum pname, GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetIntegerv("
                     << GLES2Util::GetStringGLState(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetIntegerv");
  if (GetIntegervHelper(pname, params)) {
    return;
  }
  typedef cmds::GetIntegerv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetIntegerv(pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetProgramiv(GLuint program,
                                       GLenum pname,
                                       GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramiv(" << program << ", "
                     << GLES2Util::GetStringProgramParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetProgramiv");
  if (GetProgramivHelper(program, pname, params)) {
    return;
  }
  typedef cmds::GetProgramiv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetProgramiv(program, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetProgramInfoLog(GLuint program,
                                            GLsizei bufsize,
                                            GLsizei* length,
                                            char* infolog) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramInfoLog"
                     << "(" << program << ", " << bufsize << ", "
                     << static_cast<void*>(length) << ", "
                     << static_cast<void*>(infolog) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetProgramInfoLog(program, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size = std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(infolog, str.c_str(), max_size);
      infolog[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << infolog << "\n------");
    }
  }
  if (length != nullptr) {
    *length = max_size;
  }
  CheckGLError();
}
void GLES2Implementation::GetRenderbufferParameteriv(GLenum target,
                                                     GLenum pname,
                                                     GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetRenderbufferParameteriv("
                     << GLES2Util::GetStringRenderBufferTarget(target) << ", "
                     << GLES2Util::GetStringRenderBufferParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetRenderbufferParameteriv");
  if (GetRenderbufferParameterivHelper(target, pname, params)) {
    return;
  }
  typedef cmds::GetRenderbufferParameteriv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetRenderbufferParameteriv(target, pname, GetResultShmId(),
                                      result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetSamplerParameterfv(GLuint sampler,
                                                GLenum pname,
                                                GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetSamplerParameterfv("
                     << sampler << ", "
                     << GLES2Util::GetStringSamplerParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetSamplerParameterfv");
  if (GetSamplerParameterfvHelper(sampler, pname, params)) {
    return;
  }
  typedef cmds::GetSamplerParameterfv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetSamplerParameterfv(sampler, pname, GetResultShmId(),
                                 result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetSamplerParameteriv(GLuint sampler,
                                                GLenum pname,
                                                GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetSamplerParameteriv("
                     << sampler << ", "
                     << GLES2Util::GetStringSamplerParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetSamplerParameteriv");
  if (GetSamplerParameterivHelper(sampler, pname, params)) {
    return;
  }
  typedef cmds::GetSamplerParameteriv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetSamplerParameteriv(sampler, pname, GetResultShmId(),
                                 result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetShaderiv(GLuint shader,
                                      GLenum pname,
                                      GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetShaderiv(" << shader << ", "
                     << GLES2Util::GetStringShaderParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetShaderiv");
  if (GetShaderivHelper(shader, pname, params)) {
    return;
  }
  typedef cmds::GetShaderiv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetShaderiv(shader, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetShaderInfoLog(GLuint shader,
                                           GLsizei bufsize,
                                           GLsizei* length,
                                           char* infolog) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetShaderInfoLog"
                     << "(" << shader << ", " << bufsize << ", "
                     << static_cast<void*>(length) << ", "
                     << static_cast<void*>(infolog) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetShaderInfoLog(shader, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size = std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(infolog, str.c_str(), max_size);
      infolog[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << infolog << "\n------");
    }
  }
  if (length != nullptr) {
    *length = max_size;
  }
  CheckGLError();
}
void GLES2Implementation::GetShaderSource(GLuint shader,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          char* source) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetShaderSource"
                     << "(" << shader << ", " << bufsize << ", "
                     << static_cast<void*>(length) << ", "
                     << static_cast<void*>(source) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetShaderSource(shader, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size = std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(source, str.c_str(), max_size);
      source[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << source << "\n------");
    }
  }
  if (length != nullptr) {
    *length = max_size;
  }
  CheckGLError();
}
void GLES2Implementation::GetSynciv(GLsync sync,
                                    GLenum pname,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(GLsizei, length);
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, values);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetSynciv(" << sync << ", "
                     << GLES2Util::GetStringSyncParameter(pname) << ", "
                     << bufsize << ", " << static_cast<const void*>(length)
                     << ", " << static_cast<const void*>(values) << ")");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetSynciv", "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2Implementation::GetSynciv");
  if (GetSyncivHelper(sync, pname, bufsize, length, values)) {
    return;
  }
  typedef cmds::GetSynciv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetSynciv(ToGLuint(sync), pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(values);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  if (length) {
    *length = result->GetNumResults();
  }
  CheckGLError();
}
void GLES2Implementation::GetTexParameterfv(GLenum target,
                                            GLenum pname,
                                            GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetTexParameterfv("
                     << GLES2Util::GetStringGetTexParamTarget(target) << ", "
                     << GLES2Util::GetStringTextureParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetTexParameterfv");
  if (GetTexParameterfvHelper(target, pname, params)) {
    return;
  }
  typedef cmds::GetTexParameterfv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetTexParameterfv(target, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetTexParameteriv(GLenum target,
                                            GLenum pname,
                                            GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetTexParameteriv("
                     << GLES2Util::GetStringGetTexParamTarget(target) << ", "
                     << GLES2Util::GetStringTextureParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetTexParameteriv");
  if (GetTexParameterivHelper(target, pname, params)) {
    return;
  }
  typedef cmds::GetTexParameteriv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetTexParameteriv(target, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::Hint(GLenum target, GLenum mode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glHint("
                     << GLES2Util::GetStringHintTarget(target) << ", "
                     << GLES2Util::GetStringHintMode(mode) << ")");
  helper_->Hint(target, mode);
  CheckGLError();
}

void GLES2Implementation::InvalidateFramebuffer(GLenum target,
                                                GLsizei count,
                                                const GLenum* attachments) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glInvalidateFramebuffer("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << count << ", " << static_cast<const void*>(attachments)
                     << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << attachments[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glInvalidateFramebuffer", "count < 0");
    return;
  }
  helper_->InvalidateFramebufferImmediate(target, count, attachments);
  CheckGLError();
}

void GLES2Implementation::InvalidateSubFramebuffer(GLenum target,
                                                   GLsizei count,
                                                   const GLenum* attachments,
                                                   GLint x,
                                                   GLint y,
                                                   GLsizei width,
                                                   GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glInvalidateSubFramebuffer("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << count << ", " << static_cast<const void*>(attachments)
                     << ", " << x << ", " << y << ", " << width << ", "
                     << height << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << attachments[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glInvalidateSubFramebuffer", "count < 0");
    return;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glInvalidateSubFramebuffer", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glInvalidateSubFramebuffer", "height < 0");
    return;
  }
  helper_->InvalidateSubFramebufferImmediate(target, count, attachments, x, y,
                                             width, height);
  CheckGLError();
}

GLboolean GLES2Implementation::IsBuffer(GLuint buffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsBuffer");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsBuffer(" << buffer << ")");
  typedef cmds::IsBuffer::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsBuffer(buffer, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsFramebuffer(GLuint framebuffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsFramebuffer");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsFramebuffer(" << framebuffer
                     << ")");
  typedef cmds::IsFramebuffer::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsFramebuffer(framebuffer, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsProgram(GLuint program) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsProgram");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsProgram(" << program << ")");
  typedef cmds::IsProgram::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsProgram(program, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsRenderbuffer(GLuint renderbuffer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsRenderbuffer");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsRenderbuffer(" << renderbuffer
                     << ")");
  typedef cmds::IsRenderbuffer::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsRenderbuffer(renderbuffer, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsSampler(GLuint sampler) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsSampler");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsSampler(" << sampler << ")");
  typedef cmds::IsSampler::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsSampler(sampler, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsShader(GLuint shader) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsShader");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsShader(" << shader << ")");
  typedef cmds::IsShader::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsShader(shader, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsSync(GLsync sync) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsSync");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsSync(" << sync << ")");
  typedef cmds::IsSync::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsSync(ToGLuint(sync), GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsTexture(GLuint texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsTexture");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsTexture(" << texture << ")");
  typedef cmds::IsTexture::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsTexture(texture, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

GLboolean GLES2Implementation::IsTransformFeedback(GLuint transformfeedback) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsTransformFeedback");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsTransformFeedback("
                     << transformfeedback << ")");
  typedef cmds::IsTransformFeedback::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsTransformFeedback(transformfeedback, GetResultShmId(),
                               result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

void GLES2Implementation::LineWidth(GLfloat width) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glLineWidth(" << width << ")");
  helper_->LineWidth(width);
  CheckGLError();
}

void GLES2Implementation::PauseTransformFeedback() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glPauseTransformFeedback("
                     << ")");
  helper_->PauseTransformFeedback();
  CheckGLError();
}

void GLES2Implementation::PolygonOffset(GLfloat factor, GLfloat units) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glPolygonOffset(" << factor << ", "
                     << units << ")");
  helper_->PolygonOffset(factor, units);
  CheckGLError();
}

void GLES2Implementation::ReadBuffer(GLenum src) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glReadBuffer("
                     << GLES2Util::GetStringReadBuffer(src) << ")");
  helper_->ReadBuffer(src);
  CheckGLError();
}

void GLES2Implementation::ReleaseShaderCompiler() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glReleaseShaderCompiler("
                     << ")");
  helper_->ReleaseShaderCompiler();
  CheckGLError();
}

void GLES2Implementation::RenderbufferStorage(GLenum target,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glRenderbufferStorage("
                     << GLES2Util::GetStringRenderBufferTarget(target) << ", "
                     << GLES2Util::GetStringRenderBufferFormat(internalformat)
                     << ", " << width << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage", "height < 0");
    return;
  }
  helper_->RenderbufferStorage(target, internalformat, width, height);
  CheckGLError();
}

void GLES2Implementation::ResumeTransformFeedback() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glResumeTransformFeedback("
                     << ")");
  helper_->ResumeTransformFeedback();
  CheckGLError();
}

void GLES2Implementation::SampleCoverage(GLclampf value, GLboolean invert) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSampleCoverage(" << value << ", "
                     << GLES2Util::GetStringBool(invert) << ")");
  helper_->SampleCoverage(value, invert);
  CheckGLError();
}

void GLES2Implementation::SamplerParameterf(GLuint sampler,
                                            GLenum pname,
                                            GLfloat param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSamplerParameterf(" << sampler
                     << ", " << GLES2Util::GetStringSamplerParameter(pname)
                     << ", " << param << ")");
  helper_->SamplerParameterf(sampler, pname, param);
  CheckGLError();
}

void GLES2Implementation::SamplerParameterfv(GLuint sampler,
                                             GLenum pname,
                                             const GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSamplerParameterfv(" << sampler
                     << ", " << GLES2Util::GetStringSamplerParameter(pname)
                     << ", " << static_cast<const void*>(params) << ")");
  uint32_t count = 1;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << params[ii]);
  helper_->SamplerParameterfvImmediate(sampler, pname, params);
  CheckGLError();
}

void GLES2Implementation::SamplerParameteri(GLuint sampler,
                                            GLenum pname,
                                            GLint param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSamplerParameteri(" << sampler
                     << ", " << GLES2Util::GetStringSamplerParameter(pname)
                     << ", " << param << ")");
  helper_->SamplerParameteri(sampler, pname, param);
  CheckGLError();
}

void GLES2Implementation::SamplerParameteriv(GLuint sampler,
                                             GLenum pname,
                                             const GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSamplerParameteriv(" << sampler
                     << ", " << GLES2Util::GetStringSamplerParameter(pname)
                     << ", " << static_cast<const void*>(params) << ")");
  uint32_t count = 1;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << params[ii]);
  helper_->SamplerParameterivImmediate(sampler, pname, params);
  CheckGLError();
}

void GLES2Implementation::Scissor(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glScissor(" << x << ", " << y
                     << ", " << width << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor", "height < 0");
    return;
  }
  helper_->Scissor(x, y, width, height);
  CheckGLError();
}

void GLES2Implementation::ShaderSource(GLuint shader,
                                       GLsizei count,
                                       const GLchar* const* str,
                                       const GLint* length) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glShaderSource(" << shader << ", "
                     << count << ", " << static_cast<const void*>(str) << ", "
                     << static_cast<const void*>(length) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (str[ii]) {
        if (length && length[ii] >= 0) {
          const std::string my_str(str[ii], length[ii]);
          GPU_CLIENT_LOG("  " << ii << ": ---\n" << my_str << "\n---");
        } else {
          GPU_CLIENT_LOG("  " << ii << ": ---\n" << str[ii] << "\n---");
        }
      } else {
        GPU_CLIENT_LOG("  " << ii << ": NULL");
      }
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderSource", "count < 0");
    return;
  }

  if (!PackStringsToBucket(count, str, length, "glShaderSource")) {
    return;
  }
  helper_->ShaderSourceBucket(shader, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

void GLES2Implementation::StencilFunc(GLenum func, GLint ref, GLuint mask) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glStencilFunc("
                     << GLES2Util::GetStringCmpFunction(func) << ", " << ref
                     << ", " << mask << ")");
  helper_->StencilFunc(func, ref, mask);
  CheckGLError();
}

void GLES2Implementation::StencilFuncSeparate(GLenum face,
                                              GLenum func,
                                              GLint ref,
                                              GLuint mask) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glStencilFuncSeparate("
                     << GLES2Util::GetStringFaceType(face) << ", "
                     << GLES2Util::GetStringCmpFunction(func) << ", " << ref
                     << ", " << mask << ")");
  helper_->StencilFuncSeparate(face, func, ref, mask);
  CheckGLError();
}

void GLES2Implementation::StencilMask(GLuint mask) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glStencilMask(" << mask << ")");
  helper_->StencilMask(mask);
  CheckGLError();
}

void GLES2Implementation::StencilMaskSeparate(GLenum face, GLuint mask) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glStencilMaskSeparate("
                     << GLES2Util::GetStringFaceType(face) << ", " << mask
                     << ")");
  helper_->StencilMaskSeparate(face, mask);
  CheckGLError();
}

void GLES2Implementation::StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glStencilOp("
                     << GLES2Util::GetStringStencilOp(fail) << ", "
                     << GLES2Util::GetStringStencilOp(zfail) << ", "
                     << GLES2Util::GetStringStencilOp(zpass) << ")");
  helper_->StencilOp(fail, zfail, zpass);
  CheckGLError();
}

void GLES2Implementation::StencilOpSeparate(GLenum face,
                                            GLenum fail,
                                            GLenum zfail,
                                            GLenum zpass) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glStencilOpSeparate("
                     << GLES2Util::GetStringFaceType(face) << ", "
                     << GLES2Util::GetStringStencilOp(fail) << ", "
                     << GLES2Util::GetStringStencilOp(zfail) << ", "
                     << GLES2Util::GetStringStencilOp(zpass) << ")");
  helper_->StencilOpSeparate(face, fail, zfail, zpass);
  CheckGLError();
}

void GLES2Implementation::TexParameterf(GLenum target,
                                        GLenum pname,
                                        GLfloat param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexParameterf("
                     << GLES2Util::GetStringTextureBindTarget(target) << ", "
                     << GLES2Util::GetStringTextureParameter(pname) << ", "
                     << param << ")");
  helper_->TexParameterf(target, pname, param);
  CheckGLError();
}

void GLES2Implementation::TexParameterfv(GLenum target,
                                         GLenum pname,
                                         const GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexParameterfv("
                     << GLES2Util::GetStringTextureBindTarget(target) << ", "
                     << GLES2Util::GetStringTextureParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  uint32_t count = 1;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << params[ii]);
  helper_->TexParameterfvImmediate(target, pname, params);
  CheckGLError();
}

void GLES2Implementation::TexParameteri(GLenum target,
                                        GLenum pname,
                                        GLint param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexParameteri("
                     << GLES2Util::GetStringTextureBindTarget(target) << ", "
                     << GLES2Util::GetStringTextureParameter(pname) << ", "
                     << param << ")");
  helper_->TexParameteri(target, pname, param);
  CheckGLError();
}

void GLES2Implementation::TexParameteriv(GLenum target,
                                         GLenum pname,
                                         const GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexParameteriv("
                     << GLES2Util::GetStringTextureBindTarget(target) << ", "
                     << GLES2Util::GetStringTextureParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  uint32_t count = 1;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << params[ii]);
  helper_->TexParameterivImmediate(target, pname, params);
  CheckGLError();
}

void GLES2Implementation::TexStorage3D(GLenum target,
                                       GLsizei levels,
                                       GLenum internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glTexStorage3D("
          << GLES2Util::GetStringTexture3DTarget(target) << ", " << levels
          << ", "
          << GLES2Util::GetStringTextureInternalFormatStorage(internalFormat)
          << ", " << width << ", " << height << ", " << depth << ")");
  if (levels < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage3D", "levels < 0");
    return;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage3D", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage3D", "height < 0");
    return;
  }
  if (depth < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage3D", "depth < 0");
    return;
  }
  helper_->TexStorage3D(target, levels, internalFormat, width, height, depth);
  CheckGLError();
}

void GLES2Implementation::TransformFeedbackVaryings(GLuint program,
                                                    GLsizei count,
                                                    const char* const* varyings,
                                                    GLenum buffermode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTransformFeedbackVaryings("
                     << program << ", " << count << ", "
                     << static_cast<const void*>(varyings) << ", "
                     << GLES2Util::GetStringBufferMode(buffermode) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (varyings[ii]) {
        GPU_CLIENT_LOG("  " << ii << ": ---\n" << varyings[ii] << "\n---");
      } else {
        GPU_CLIENT_LOG("  " << ii << ": NULL");
      }
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glTransformFeedbackVaryings", "count < 0");
    return;
  }

  if (!PackStringsToBucket(count, varyings, nullptr,
                           "glTransformFeedbackVaryings")) {
    return;
  }
  helper_->TransformFeedbackVaryingsBucket(program, kResultBucketId,
                                           buffermode);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

void GLES2Implementation::Uniform1f(GLint location, GLfloat x) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform1f(" << location << ", "
                     << x << ")");
  helper_->Uniform1f(location, x);
  CheckGLError();
}

void GLES2Implementation::Uniform1fv(GLint location,
                                     GLsizei count,
                                     const GLfloat* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform1fv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1fv", "count < 0");
    return;
  }
  helper_->Uniform1fvImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform1i(GLint location, GLint x) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform1i(" << location << ", "
                     << x << ")");
  helper_->Uniform1i(location, x);
  CheckGLError();
}

void GLES2Implementation::Uniform1iv(GLint location,
                                     GLsizei count,
                                     const GLint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform1iv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1iv", "count < 0");
    return;
  }
  helper_->Uniform1ivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform1ui(GLint location, GLuint x) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform1ui(" << location << ", "
                     << x << ")");
  helper_->Uniform1ui(location, x);
  CheckGLError();
}

void GLES2Implementation::Uniform1uiv(GLint location,
                                      GLsizei count,
                                      const GLuint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform1uiv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1uiv", "count < 0");
    return;
  }
  helper_->Uniform1uivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform2f(GLint location, GLfloat x, GLfloat y) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform2f(" << location << ", "
                     << x << ", " << y << ")");
  helper_->Uniform2f(location, x, y);
  CheckGLError();
}

void GLES2Implementation::Uniform2fv(GLint location,
                                     GLsizei count,
                                     const GLfloat* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform2fv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 2] << ", " << v[1 + i * 2]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2fv", "count < 0");
    return;
  }
  helper_->Uniform2fvImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform2i(GLint location, GLint x, GLint y) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform2i(" << location << ", "
                     << x << ", " << y << ")");
  helper_->Uniform2i(location, x, y);
  CheckGLError();
}

void GLES2Implementation::Uniform2iv(GLint location,
                                     GLsizei count,
                                     const GLint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform2iv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 2] << ", " << v[1 + i * 2]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2iv", "count < 0");
    return;
  }
  helper_->Uniform2ivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform2ui(GLint location, GLuint x, GLuint y) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform2ui(" << location << ", "
                     << x << ", " << y << ")");
  helper_->Uniform2ui(location, x, y);
  CheckGLError();
}

void GLES2Implementation::Uniform2uiv(GLint location,
                                      GLsizei count,
                                      const GLuint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform2uiv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 2] << ", " << v[1 + i * 2]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2uiv", "count < 0");
    return;
  }
  helper_->Uniform2uivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform3f(GLint location,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform3f(" << location << ", "
                     << x << ", " << y << ", " << z << ")");
  helper_->Uniform3f(location, x, y, z);
  CheckGLError();
}

void GLES2Implementation::Uniform3fv(GLint location,
                                     GLsizei count,
                                     const GLfloat* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform3fv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 3] << ", " << v[1 + i * 3]
                          << ", " << v[2 + i * 3]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3fv", "count < 0");
    return;
  }
  helper_->Uniform3fvImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform3i(" << location << ", "
                     << x << ", " << y << ", " << z << ")");
  helper_->Uniform3i(location, x, y, z);
  CheckGLError();
}

void GLES2Implementation::Uniform3iv(GLint location,
                                     GLsizei count,
                                     const GLint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform3iv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 3] << ", " << v[1 + i * 3]
                          << ", " << v[2 + i * 3]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3iv", "count < 0");
    return;
  }
  helper_->Uniform3ivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform3ui(GLint location,
                                     GLuint x,
                                     GLuint y,
                                     GLuint z) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform3ui(" << location << ", "
                     << x << ", " << y << ", " << z << ")");
  helper_->Uniform3ui(location, x, y, z);
  CheckGLError();
}

void GLES2Implementation::Uniform3uiv(GLint location,
                                      GLsizei count,
                                      const GLuint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform3uiv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 3] << ", " << v[1 + i * 3]
                          << ", " << v[2 + i * 3]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3uiv", "count < 0");
    return;
  }
  helper_->Uniform3uivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform4f(GLint location,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform4f(" << location << ", "
                     << x << ", " << y << ", " << z << ", " << w << ")");
  helper_->Uniform4f(location, x, y, z, w);
  CheckGLError();
}

void GLES2Implementation::Uniform4fv(GLint location,
                                     GLsizei count,
                                     const GLfloat* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform4fv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 4] << ", " << v[1 + i * 4]
                          << ", " << v[2 + i * 4] << ", " << v[3 + i * 4]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4fv", "count < 0");
    return;
  }
  helper_->Uniform4fvImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform4i(GLint location,
                                    GLint x,
                                    GLint y,
                                    GLint z,
                                    GLint w) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform4i(" << location << ", "
                     << x << ", " << y << ", " << z << ", " << w << ")");
  helper_->Uniform4i(location, x, y, z, w);
  CheckGLError();
}

void GLES2Implementation::Uniform4iv(GLint location,
                                     GLsizei count,
                                     const GLint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform4iv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 4] << ", " << v[1 + i * 4]
                          << ", " << v[2 + i * 4] << ", " << v[3 + i * 4]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4iv", "count < 0");
    return;
  }
  helper_->Uniform4ivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::Uniform4ui(GLint location,
                                     GLuint x,
                                     GLuint y,
                                     GLuint z,
                                     GLuint w) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform4ui(" << location << ", "
                     << x << ", " << y << ", " << z << ", " << w << ")");
  helper_->Uniform4ui(location, x, y, z, w);
  CheckGLError();
}

void GLES2Implementation::Uniform4uiv(GLint location,
                                      GLsizei count,
                                      const GLuint* v) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniform4uiv(" << location << ", "
                     << count << ", " << static_cast<const void*>(v) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 4] << ", " << v[1 + i * 4]
                          << ", " << v[2 + i * 4] << ", " << v[3 + i * 4]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4uiv", "count < 0");
    return;
  }
  helper_->Uniform4uivImmediate(location, count, v);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix2fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix2fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 4] << ", "
                          << value[1 + i * 4] << ", " << value[2 + i * 4]
                          << ", " << value[3 + i * 4]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix2fv", "count < 0");
    return;
  }
  helper_->UniformMatrix2fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix2x3fv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix2x3fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 6] << ", "
                          << value[1 + i * 6] << ", " << value[2 + i * 6]
                          << ", " << value[3 + i * 6] << ", "
                          << value[4 + i * 6] << ", " << value[5 + i * 6]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix2x3fv", "count < 0");
    return;
  }
  helper_->UniformMatrix2x3fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix2x4fv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix2x4fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG(
          "  " << i << ": " << value[0 + i * 8] << ", " << value[1 + i * 8]
               << ", " << value[2 + i * 8] << ", " << value[3 + i * 8] << ", "
               << value[4 + i * 8] << ", " << value[5 + i * 8] << ", "
               << value[6 + i * 8] << ", " << value[7 + i * 8]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix2x4fv", "count < 0");
    return;
  }
  helper_->UniformMatrix2x4fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix3fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix3fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 9] << ", "
                          << value[1 + i * 9] << ", " << value[2 + i * 9]
                          << ", " << value[3 + i * 9] << ", "
                          << value[4 + i * 9] << ", " << value[5 + i * 9]
                          << ", " << value[6 + i * 9] << ", "
                          << value[7 + i * 9] << ", " << value[8 + i * 9]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix3fv", "count < 0");
    return;
  }
  helper_->UniformMatrix3fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix3x2fv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix3x2fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 6] << ", "
                          << value[1 + i * 6] << ", " << value[2 + i * 6]
                          << ", " << value[3 + i * 6] << ", "
                          << value[4 + i * 6] << ", " << value[5 + i * 6]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix3x2fv", "count < 0");
    return;
  }
  helper_->UniformMatrix3x2fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix3x4fv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix3x4fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG(
          "  " << i << ": " << value[0 + i * 12] << ", " << value[1 + i * 12]
               << ", " << value[2 + i * 12] << ", " << value[3 + i * 12] << ", "
               << value[4 + i * 12] << ", " << value[5 + i * 12] << ", "
               << value[6 + i * 12] << ", " << value[7 + i * 12] << ", "
               << value[8 + i * 12] << ", " << value[9 + i * 12] << ", "
               << value[10 + i * 12] << ", " << value[11 + i * 12]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix3x4fv", "count < 0");
    return;
  }
  helper_->UniformMatrix3x4fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix4fv(GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix4fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG(
          "  " << i << ": " << value[0 + i * 16] << ", " << value[1 + i * 16]
               << ", " << value[2 + i * 16] << ", " << value[3 + i * 16] << ", "
               << value[4 + i * 16] << ", " << value[5 + i * 16] << ", "
               << value[6 + i * 16] << ", " << value[7 + i * 16] << ", "
               << value[8 + i * 16] << ", " << value[9 + i * 16] << ", "
               << value[10 + i * 16] << ", " << value[11 + i * 16] << ", "
               << value[12 + i * 16] << ", " << value[13 + i * 16] << ", "
               << value[14 + i * 16] << ", " << value[15 + i * 16]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix4fv", "count < 0");
    return;
  }
  helper_->UniformMatrix4fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix4x2fv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix4x2fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG(
          "  " << i << ": " << value[0 + i * 8] << ", " << value[1 + i * 8]
               << ", " << value[2 + i * 8] << ", " << value[3 + i * 8] << ", "
               << value[4 + i * 8] << ", " << value[5 + i * 8] << ", "
               << value[6 + i * 8] << ", " << value[7 + i * 8]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix4x2fv", "count < 0");
    return;
  }
  helper_->UniformMatrix4x2fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UniformMatrix4x3fv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformMatrix4x3fv(" << location
                     << ", " << count << ", "
                     << GLES2Util::GetStringBool(transpose) << ", "
                     << static_cast<const void*>(value) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG(
          "  " << i << ": " << value[0 + i * 12] << ", " << value[1 + i * 12]
               << ", " << value[2 + i * 12] << ", " << value[3 + i * 12] << ", "
               << value[4 + i * 12] << ", " << value[5 + i * 12] << ", "
               << value[6 + i * 12] << ", " << value[7 + i * 12] << ", "
               << value[8 + i * 12] << ", " << value[9 + i * 12] << ", "
               << value[10 + i * 12] << ", " << value[11 + i * 12]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix4x3fv", "count < 0");
    return;
  }
  helper_->UniformMatrix4x3fvImmediate(location, count, transpose, value);
  CheckGLError();
}

void GLES2Implementation::UseProgram(GLuint program) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUseProgram(" << program << ")");
  if (IsProgramReservedId(program)) {
    SetGLError(GL_INVALID_OPERATION, "UseProgram", "program reserved id");
    return;
  }
  UseProgramHelper(program);
  CheckGLError();
}

void GLES2Implementation::ValidateProgram(GLuint program) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glValidateProgram(" << program
                     << ")");
  helper_->ValidateProgram(program);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib1f(GLuint indx, GLfloat x) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib1f(" << indx << ", "
                     << x << ")");
  helper_->VertexAttrib1f(indx, x);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib1fv(" << indx << ", "
                     << static_cast<const void*>(values) << ")");
  uint32_t count = 1;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << values[ii]);
  helper_->VertexAttrib1fvImmediate(indx, values);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib2f(" << indx << ", "
                     << x << ", " << y << ")");
  helper_->VertexAttrib2f(indx, x, y);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib2fv(" << indx << ", "
                     << static_cast<const void*>(values) << ")");
  uint32_t count = 2;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << values[ii]);
  helper_->VertexAttrib2fvImmediate(indx, values);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib3f(GLuint indx,
                                         GLfloat x,
                                         GLfloat y,
                                         GLfloat z) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib3f(" << indx << ", "
                     << x << ", " << y << ", " << z << ")");
  helper_->VertexAttrib3f(indx, x, y, z);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib3fv(" << indx << ", "
                     << static_cast<const void*>(values) << ")");
  uint32_t count = 3;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << values[ii]);
  helper_->VertexAttrib3fvImmediate(indx, values);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib4f(GLuint indx,
                                         GLfloat x,
                                         GLfloat y,
                                         GLfloat z,
                                         GLfloat w) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib4f(" << indx << ", "
                     << x << ", " << y << ", " << z << ", " << w << ")");
  helper_->VertexAttrib4f(indx, x, y, z, w);
  CheckGLError();
}

void GLES2Implementation::VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttrib4fv(" << indx << ", "
                     << static_cast<const void*>(values) << ")");
  uint32_t count = 4;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << values[ii]);
  helper_->VertexAttrib4fvImmediate(indx, values);
  CheckGLError();
}

void GLES2Implementation::VertexAttribI4i(GLuint indx,
                                          GLint x,
                                          GLint y,
                                          GLint z,
                                          GLint w) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribI4i(" << indx << ", "
                     << x << ", " << y << ", " << z << ", " << w << ")");
  helper_->VertexAttribI4i(indx, x, y, z, w);
  CheckGLError();
}

void GLES2Implementation::VertexAttribI4iv(GLuint indx, const GLint* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribI4iv(" << indx
                     << ", " << static_cast<const void*>(values) << ")");
  uint32_t count = 4;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << values[ii]);
  helper_->VertexAttribI4ivImmediate(indx, values);
  CheckGLError();
}

void GLES2Implementation::VertexAttribI4ui(GLuint indx,
                                           GLuint x,
                                           GLuint y,
                                           GLuint z,
                                           GLuint w) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribI4ui(" << indx
                     << ", " << x << ", " << y << ", " << z << ", " << w
                     << ")");
  helper_->VertexAttribI4ui(indx, x, y, z, w);
  CheckGLError();
}

void GLES2Implementation::VertexAttribI4uiv(GLuint indx, const GLuint* values) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribI4uiv(" << indx
                     << ", " << static_cast<const void*>(values) << ")");
  uint32_t count = 4;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << values[ii]);
  helper_->VertexAttribI4uivImmediate(indx, values);
  CheckGLError();
}

void GLES2Implementation::BlitFramebufferCHROMIUM(GLint srcX0,
                                                  GLint srcY0,
                                                  GLint srcX1,
                                                  GLint srcY1,
                                                  GLint dstX0,
                                                  GLint dstY0,
                                                  GLint dstX1,
                                                  GLint dstY1,
                                                  GLbitfield mask,
                                                  GLenum filter) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlitFramebufferCHROMIUM("
                     << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1
                     << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", "
                     << dstY1 << ", " << mask << ", "
                     << GLES2Util::GetStringBlitFilter(filter) << ")");
  helper_->BlitFramebufferCHROMIUM(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                   dstX1, dstY1, mask, filter);
  CheckGLError();
}

void GLES2Implementation::RenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glRenderbufferStorageMultisampleCHROMIUM("
          << GLES2Util::GetStringRenderBufferTarget(target) << ", " << samples
          << ", " << GLES2Util::GetStringRenderBufferFormat(internalformat)
          << ", " << width << ", " << height << ")");
  if (samples < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleCHROMIUM",
               "samples < 0");
    return;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleCHROMIUM",
               "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleCHROMIUM",
               "height < 0");
    return;
  }
  helper_->RenderbufferStorageMultisampleCHROMIUM(
      target, samples, internalformat, width, height);
  CheckGLError();
}

void GLES2Implementation::RenderbufferStorageMultisampleAdvancedAMD(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glRenderbufferStorageMultisampleAdvancedAMD("
                     << GLES2Util::GetStringRenderBufferTarget(target) << ", "
                     << samples << ", " << storageSamples << ", "
                     << GLES2Util::GetStringRenderBufferFormat(internalformat)
                     << ", " << width << ", " << height << ")");
  if (samples < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleAdvancedAMD",
               "samples < 0");
    return;
  }
  if (storageSamples < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleAdvancedAMD",
               "storageSamples < 0");
    return;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleAdvancedAMD",
               "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleAdvancedAMD",
               "height < 0");
    return;
  }
  helper_->RenderbufferStorageMultisampleAdvancedAMD(
      target, samples, storageSamples, internalformat, width, height);
  CheckGLError();
}

void GLES2Implementation::RenderbufferStorageMultisampleEXT(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glRenderbufferStorageMultisampleEXT("
          << GLES2Util::GetStringRenderBufferTarget(target) << ", " << samples
          << ", " << GLES2Util::GetStringRenderBufferFormat(internalformat)
          << ", " << width << ", " << height << ")");
  if (samples < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT",
               "samples < 0");
    return;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT",
               "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT",
               "height < 0");
    return;
  }
  helper_->RenderbufferStorageMultisampleEXT(target, samples, internalformat,
                                             width, height);
  CheckGLError();
}

void GLES2Implementation::FramebufferTexture2DMultisampleEXT(GLenum target,
                                                             GLenum attachment,
                                                             GLenum textarget,
                                                             GLuint texture,
                                                             GLint level,
                                                             GLsizei samples) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferTexture2DMultisampleEXT("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << GLES2Util::GetStringAttachment(attachment) << ", "
                     << GLES2Util::GetStringTextureTarget(textarget) << ", "
                     << texture << ", " << level << ", " << samples << ")");
  if (samples < 0) {
    SetGLError(GL_INVALID_VALUE, "glFramebufferTexture2DMultisampleEXT",
               "samples < 0");
    return;
  }
  helper_->FramebufferTexture2DMultisampleEXT(target, attachment, textarget,
                                              texture, level, samples);
  CheckGLError();
}

void GLES2Implementation::TexStorage2DEXT(GLenum target,
                                          GLsizei levels,
                                          GLenum internalFormat,
                                          GLsizei width,
                                          GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glTexStorage2DEXT("
          << GLES2Util::GetStringTextureBindTarget(target) << ", " << levels
          << ", "
          << GLES2Util::GetStringTextureInternalFormatStorage(internalFormat)
          << ", " << width << ", " << height << ")");
  if (levels < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT", "levels < 0");
    return;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT", "height < 0");
    return;
  }
  helper_->TexStorage2DEXT(target, levels, internalFormat, width, height);
  CheckGLError();
}

void GLES2Implementation::GenQueriesEXT(GLsizei n, GLuint* queries) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenQueriesEXT(" << n << ", "
                     << static_cast<const void*>(queries) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenQueriesEXT", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kQueries);
  for (GLsizei ii = 0; ii < n; ++ii)
    queries[ii] = id_allocator->AllocateID();
  GenQueriesEXTHelper(n, queries);
  helper_->GenQueriesEXTImmediate(n, queries);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << queries[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::DeleteQueriesEXT(GLsizei n, const GLuint* queries) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteQueriesEXT(" << n << ", "
                     << static_cast<const void*>(queries) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << queries[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(queries[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteQueriesEXT", "n < 0");
    return;
  }
  DeleteQueriesEXTHelper(n, queries);
  CheckGLError();
}

void GLES2Implementation::BeginTransformFeedback(GLenum primitivemode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glBeginTransformFeedback("
          << GLES2Util::GetStringTransformFeedbackPrimitiveMode(primitivemode)
          << ")");
  helper_->BeginTransformFeedback(primitivemode);
  CheckGLError();
}

void GLES2Implementation::EndTransformFeedback() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glEndTransformFeedback("
                     << ")");
  helper_->EndTransformFeedback();
  CheckGLError();
}

void GLES2Implementation::GenVertexArraysOES(GLsizei n, GLuint* arrays) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenVertexArraysOES(" << n << ", "
                     << static_cast<const void*>(arrays) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenVertexArraysOES", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kVertexArrays);
  for (GLsizei ii = 0; ii < n; ++ii)
    arrays[ii] = id_allocator->AllocateID();
  GenVertexArraysOESHelper(n, arrays);
  helper_->GenVertexArraysOESImmediate(n, arrays);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << arrays[i]);
    }
  });
  CheckGLError();
}

void GLES2Implementation::DeleteVertexArraysOES(GLsizei n,
                                                const GLuint* arrays) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteVertexArraysOES(" << n
                     << ", " << static_cast<const void*>(arrays) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << arrays[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(arrays[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteVertexArraysOES", "n < 0");
    return;
  }
  DeleteVertexArraysOESHelper(n, arrays);
  CheckGLError();
}

GLboolean GLES2Implementation::IsVertexArrayOES(GLuint array) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2Implementation::IsVertexArrayOES");
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsVertexArrayOES(" << array
                     << ")");
  typedef cmds::IsVertexArrayOES::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return GL_FALSE;
  }
  *result = 0;
  helper_->IsVertexArrayOES(array, GetResultShmId(), result.offset());
  WaitForCmd();
  GLboolean result_value = *result != 0;
  GPU_CLIENT_LOG("returned " << result_value);
  CheckGLError();
  return result_value;
}

void GLES2Implementation::BindVertexArrayOES(GLuint array) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindVertexArrayOES(" << array
                     << ")");
  if (IsVertexArrayReservedId(array)) {
    SetGLError(GL_INVALID_OPERATION, "BindVertexArrayOES", "array reserved id");
    return;
  }
  BindVertexArrayOESHelper(array);
  CheckGLError();
}

void GLES2Implementation::FramebufferParameteri(GLenum target,
                                                GLenum pname,
                                                GLint param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFramebufferParameteri("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << GLES2Util::GetStringFramebufferParameter(pname) << ", "
                     << param << ")");
  helper_->FramebufferParameteri(target, pname, param);
  CheckGLError();
}

void GLES2Implementation::BindImageTexture(GLuint unit,
                                           GLuint texture,
                                           GLint level,
                                           GLboolean layered,
                                           GLint layer,
                                           GLenum access,
                                           GLenum format) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindImageTexture(" << unit
                     << ", " << texture << ", " << level << ", "
                     << GLES2Util::GetStringBool(layered) << ", " << layer
                     << ", " << GLES2Util::GetStringEnum(access) << ", "
                     << GLES2Util::GetStringEnum(format) << ")");
  helper_->BindImageTexture(unit, texture, level, layered, layer, access,
                            format);
  CheckGLError();
}

void GLES2Implementation::DispatchCompute(GLuint num_groups_x,
                                          GLuint num_groups_y,
                                          GLuint num_groups_z) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDispatchCompute(" << num_groups_x
                     << ", " << num_groups_y << ", " << num_groups_z << ")");
  helper_->DispatchCompute(num_groups_x, num_groups_y, num_groups_z);
  CheckGLError();
}

void GLES2Implementation::DispatchComputeIndirect(GLintptr offset) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDispatchComputeIndirect("
                     << offset << ")");
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE, "glDispatchComputeIndirect", "offset < 0");
    return;
  }
  helper_->DispatchComputeIndirect(offset);
  CheckGLError();
}

void GLES2Implementation::GetProgramInterfaceiv(GLuint program,
                                                GLenum program_interface,
                                                GLenum pname,
                                                GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramInterfaceiv("
                     << program << ", "
                     << GLES2Util::GetStringEnum(program_interface) << ", "
                     << GLES2Util::GetStringEnum(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2Implementation::GetProgramInterfaceiv");
  if (GetProgramInterfaceivHelper(program, program_interface, pname, params)) {
    return;
  }
  typedef cmds::GetProgramInterfaceiv::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetProgramInterfaceiv(program, program_interface, pname,
                                 GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::MemoryBarrierEXT(GLbitfield barriers) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMemoryBarrierEXT(" << barriers
                     << ")");
  helper_->MemoryBarrierEXT(barriers);
  CheckGLError();
}

void GLES2Implementation::MemoryBarrierByRegion(GLbitfield barriers) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMemoryBarrierByRegion("
                     << barriers << ")");
  helper_->MemoryBarrierByRegion(barriers);
  CheckGLError();
}

void GLES2Implementation::FlushMappedBufferRange(GLenum target,
                                                 GLintptr offset,
                                                 GLsizeiptr size) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFlushMappedBufferRange("
                     << GLES2Util::GetStringBufferTarget(target) << ", "
                     << offset << ", " << size << ")");
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE, "glFlushMappedBufferRange", "offset < 0");
    return;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glFlushMappedBufferRange", "size < 0");
    return;
  }
  helper_->FlushMappedBufferRange(target, offset, size);
  CheckGLError();
}

void GLES2Implementation::DescheduleUntilFinishedCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDescheduleUntilFinishedCHROMIUM("
                     << ")");
  helper_->DescheduleUntilFinishedCHROMIUM();
  CheckGLError();
}

void GLES2Implementation::GetTranslatedShaderSourceANGLE(GLuint shader,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         char* source) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetTranslatedShaderSourceANGLE"
                     << "(" << shader << ", " << bufsize << ", "
                     << static_cast<void*>(length) << ", "
                     << static_cast<void*>(source) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetTranslatedShaderSourceANGLE(shader, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size = std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(source, str.c_str(), max_size);
      source[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << source << "\n------");
    }
  }
  if (length != nullptr) {
    *length = max_size;
  }
  CheckGLError();
}
void GLES2Implementation::CopyTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint internalformat,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCopyTextureCHROMIUM(" << source_id << ", "
          << source_level << ", "
          << GLES2Util::GetStringTextureTarget(dest_target) << ", " << dest_id
          << ", " << dest_level << ", " << internalformat << ", "
          << GLES2Util::GetStringPixelType(dest_type) << ", "
          << GLES2Util::GetStringBool(unpack_flip_y) << ", "
          << GLES2Util::GetStringBool(unpack_premultiply_alpha) << ", "
          << GLES2Util::GetStringBool(unpack_unmultiply_alpha) << ")");
  helper_->CopyTextureCHROMIUM(source_id, source_level, dest_target, dest_id,
                               dest_level, internalformat, dest_type,
                               unpack_flip_y, unpack_premultiply_alpha,
                               unpack_unmultiply_alpha);
  CheckGLError();
}

void GLES2Implementation::CopySubTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCopySubTextureCHROMIUM(" << source_id
          << ", " << source_level << ", "
          << GLES2Util::GetStringTextureTarget(dest_target) << ", " << dest_id
          << ", " << dest_level << ", " << xoffset << ", " << yoffset << ", "
          << x << ", " << y << ", " << width << ", " << height << ", "
          << GLES2Util::GetStringBool(unpack_flip_y) << ", "
          << GLES2Util::GetStringBool(unpack_premultiply_alpha) << ", "
          << GLES2Util::GetStringBool(unpack_unmultiply_alpha) << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySubTextureCHROMIUM", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySubTextureCHROMIUM", "height < 0");
    return;
  }
  helper_->CopySubTextureCHROMIUM(
      source_id, source_level, dest_target, dest_id, dest_level, xoffset,
      yoffset, x, y, width, height, unpack_flip_y, unpack_premultiply_alpha,
      unpack_unmultiply_alpha);
  CheckGLError();
}

void GLES2Implementation::DiscardFramebufferEXT(GLenum target,
                                                GLsizei count,
                                                const GLenum* attachments) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDiscardFramebufferEXT("
                     << GLES2Util::GetStringFramebufferTarget(target) << ", "
                     << count << ", " << static_cast<const void*>(attachments)
                     << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << attachments[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDiscardFramebufferEXT", "count < 0");
    return;
  }
  helper_->DiscardFramebufferEXTImmediate(target, count, attachments);
  CheckGLError();
}

void GLES2Implementation::LoseContextCHROMIUM(GLenum current, GLenum other) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glLoseContextCHROMIUM("
                     << GLES2Util::GetStringResetStatus(current) << ", "
                     << GLES2Util::GetStringResetStatus(other) << ")");
  helper_->LoseContextCHROMIUM(current, other);
  CheckGLError();
}

void GLES2Implementation::DrawBuffersEXT(GLsizei count, const GLenum* bufs) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawBuffersEXT(" << count << ", "
                     << static_cast<const void*>(bufs) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << bufs[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawBuffersEXT", "count < 0");
    return;
  }
  helper_->DrawBuffersEXTImmediate(count, bufs);
  CheckGLError();
}

void GLES2Implementation::DiscardBackbufferCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDiscardBackbufferCHROMIUM("
                     << ")");
  helper_->DiscardBackbufferCHROMIUM();
  CheckGLError();
}

void GLES2Implementation::FlushDriverCachesCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFlushDriverCachesCHROMIUM("
                     << ")");
  helper_->FlushDriverCachesCHROMIUM();
  CheckGLError();
}

void GLES2Implementation::ContextVisibilityHintCHROMIUM(GLboolean visibility) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glContextVisibilityHintCHROMIUM("
                     << GLES2Util::GetStringBool(visibility) << ")");
  helper_->ContextVisibilityHintCHROMIUM(visibility);
  CheckGLError();
}

void GLES2Implementation::BlendBarrierKHR() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendBarrierKHR("
                     << ")");
  helper_->BlendBarrierKHR();
  CheckGLError();
}

void GLES2Implementation::WindowRectanglesEXT(GLenum mode,
                                              GLsizei count,
                                              const GLint* box) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glWindowRectanglesEXT("
                     << GLES2Util::GetStringWindowRectanglesMode(mode) << ", "
                     << count << ", " << static_cast<const void*>(box) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << box[0 + i * 4] << ", "
                          << box[1 + i * 4] << ", " << box[2 + i * 4] << ", "
                          << box[3 + i * 4]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glWindowRectanglesEXT", "count < 0");
    return;
  }
  helper_->WindowRectanglesEXTImmediate(mode, count, box);
  CheckGLError();
}

void GLES2Implementation::WaitGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glWaitGpuFenceCHROMIUM("
                     << gpu_fence_id << ")");
  helper_->WaitGpuFenceCHROMIUM(gpu_fence_id);
  CheckGLError();
}

void GLES2Implementation::DestroyGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDestroyGpuFenceCHROMIUM("
                     << gpu_fence_id << ")");
  helper_->DestroyGpuFenceCHROMIUM(gpu_fence_id);
  CheckGLError();
}

void GLES2Implementation::FramebufferTextureMultiviewOVR(GLenum target,
                                                         GLenum attachment,
                                                         GLuint texture,
                                                         GLint level,
                                                         GLint baseViewIndex,
                                                         GLsizei numViews) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFramebufferTextureMultiviewOVR("
                     << GLES2Util::GetStringEnum(target) << ", "
                     << GLES2Util::GetStringEnum(attachment) << ", " << texture
                     << ", " << level << ", " << baseViewIndex << ", "
                     << numViews << ")");
  if (numViews < 0) {
    SetGLError(GL_INVALID_VALUE, "glFramebufferTextureMultiviewOVR",
               "numViews < 0");
    return;
  }
  helper_->FramebufferTextureMultiviewOVR(target, attachment, texture, level,
                                          baseViewIndex, numViews);
  CheckGLError();
}

void GLES2Implementation::MaxShaderCompilerThreadsKHR(GLuint count) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMaxShaderCompilerThreadsKHR("
                     << count << ")");
  helper_->MaxShaderCompilerThreadsKHR(count);
  CheckGLError();
}

void GLES2Implementation::BeginSharedImageAccessDirectCHROMIUM(GLuint texture,
                                                               GLenum mode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glBeginSharedImageAccessDirectCHROMIUM(" << texture
                     << ", " << GLES2Util::GetStringSharedImageAccessMode(mode)
                     << ")");
  helper_->BeginSharedImageAccessDirectCHROMIUM(texture, mode);
  CheckGLError();
}

void GLES2Implementation::EndSharedImageAccessDirectCHROMIUM(GLuint texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glEndSharedImageAccessDirectCHROMIUM(" << texture
                     << ")");
  helper_->EndSharedImageAccessDirectCHROMIUM(texture);
  CheckGLError();
}

void GLES2Implementation::ConvertRGBAToYUVAMailboxesINTERNAL(
    GLenum planes_yuv_color_space,
    GLenum plane_config,
    GLenum subsampling,
    const GLbyte* mailboxes) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glConvertRGBAToYUVAMailboxesINTERNAL("
                     << GLES2Util::GetStringEnum(planes_yuv_color_space) << ", "
                     << GLES2Util::GetStringEnum(plane_config) << ", "
                     << GLES2Util::GetStringEnum(subsampling) << ", "
                     << static_cast<const void*>(mailboxes) << ")");
  uint32_t count = 80;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << mailboxes[ii]);
  helper_->ConvertRGBAToYUVAMailboxesINTERNALImmediate(
      planes_yuv_color_space, plane_config, subsampling, mailboxes);
  CheckGLError();
}

void GLES2Implementation::ConvertYUVAMailboxesToRGBINTERNAL(
    GLenum planes_yuv_color_space,
    GLenum plane_config,
    GLenum subsampling,
    const GLbyte* mailboxes) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glConvertYUVAMailboxesToRGBINTERNAL("
                     << GLES2Util::GetStringEnum(planes_yuv_color_space) << ", "
                     << GLES2Util::GetStringEnum(plane_config) << ", "
                     << GLES2Util::GetStringEnum(subsampling) << ", "
                     << static_cast<const void*>(mailboxes) << ")");
  uint32_t count = 144;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << mailboxes[ii]);
  helper_->ConvertYUVAMailboxesToRGBINTERNALImmediate(
      planes_yuv_color_space, plane_config, subsampling, mailboxes);
  CheckGLError();
}

void GLES2Implementation::CopySharedImageINTERNAL(GLint xoffset,
                                                  GLint yoffset,
                                                  GLint x,
                                                  GLint y,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLboolean unpack_flip_y,
                                                  const GLbyte* mailboxes) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCopySharedImageINTERNAL("
                     << xoffset << ", " << yoffset << ", " << x << ", " << y
                     << ", " << width << ", " << height << ", "
                     << GLES2Util::GetStringBool(unpack_flip_y) << ", "
                     << static_cast<const void*>(mailboxes) << ")");
  uint32_t count = 32;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << mailboxes[ii]);
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySharedImageINTERNAL", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySharedImageINTERNAL", "height < 0");
    return;
  }
  helper_->CopySharedImageINTERNALImmediate(xoffset, yoffset, x, y, width,
                                            height, unpack_flip_y, mailboxes);
  CheckGLError();
}

void GLES2Implementation::CopySharedImageToTextureINTERNAL(
    GLuint texture,
    GLenum target,
    GLuint internal_format,
    GLenum type,
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    GLboolean flip_y,
    const GLbyte* src_mailbox) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCopySharedImageToTextureINTERNAL("
          << texture << ", " << GLES2Util::GetStringEnum(target) << ", "
          << internal_format << ", " << GLES2Util::GetStringEnum(type) << ", "
          << src_x << ", " << src_y << ", " << width << ", " << height << ", "
          << GLES2Util::GetStringBool(flip_y) << ", "
          << static_cast<const void*>(src_mailbox) << ")");
  uint32_t count = 16;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << src_mailbox[ii]);
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySharedImageToTextureINTERNAL",
               "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySharedImageToTextureINTERNAL",
               "height < 0");
    return;
  }
  helper_->CopySharedImageToTextureINTERNALImmediate(
      texture, target, internal_format, type, src_x, src_y, width, height,
      flip_y, src_mailbox);
  CheckGLError();
}

void GLES2Implementation::BlendEquationiOES(GLuint buf, GLenum mode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendEquationiOES(" << buf
                     << ", " << GLES2Util::GetStringEnum(mode) << ")");
  helper_->BlendEquationiOES(buf, mode);
  CheckGLError();
}

void GLES2Implementation::BlendEquationSeparateiOES(GLuint buf,
                                                    GLenum modeRGB,
                                                    GLenum modeAlpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendEquationSeparateiOES("
                     << buf << ", " << GLES2Util::GetStringEnum(modeRGB) << ", "
                     << GLES2Util::GetStringEnum(modeAlpha) << ")");
  helper_->BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
  CheckGLError();
}

void GLES2Implementation::BlendFunciOES(GLuint buf, GLenum src, GLenum dst) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendFunciOES(" << buf << ", "
                     << GLES2Util::GetStringEnum(src) << ", "
                     << GLES2Util::GetStringEnum(dst) << ")");
  helper_->BlendFunciOES(buf, src, dst);
  CheckGLError();
}

void GLES2Implementation::BlendFuncSeparateiOES(GLuint buf,
                                                GLenum srcRGB,
                                                GLenum dstRGB,
                                                GLenum srcAlpha,
                                                GLenum dstAlpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBlendFuncSeparateiOES(" << buf
                     << ", " << GLES2Util::GetStringEnum(srcRGB) << ", "
                     << GLES2Util::GetStringEnum(dstRGB) << ", "
                     << GLES2Util::GetStringEnum(srcAlpha) << ", "
                     << GLES2Util::GetStringEnum(dstAlpha) << ")");
  helper_->BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
  CheckGLError();
}

void GLES2Implementation::ColorMaskiOES(GLuint buf,
                                        GLboolean r,
                                        GLboolean g,
                                        GLboolean b,
                                        GLboolean a) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glColorMaskiOES(" << buf << ", "
                     << GLES2Util::GetStringBool(r) << ", "
                     << GLES2Util::GetStringBool(g) << ", "
                     << GLES2Util::GetStringBool(b) << ", "
                     << GLES2Util::GetStringBool(a) << ")");
  helper_->ColorMaskiOES(buf, r, g, b, a);
  CheckGLError();
}

void GLES2Implementation::ProvokingVertexANGLE(GLenum provokeMode) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glProvokingVertexANGLE("
                     << GLES2Util::GetStringEnum(provokeMode) << ")");
  helper_->ProvokingVertexANGLE(provokeMode);
  CheckGLError();
}

void GLES2Implementation::FramebufferMemorylessPixelLocalStorageANGLE(
    GLint plane,
    GLenum internalformat) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferMemorylessPixelLocalStorageANGLE("
                     << plane << ", "
                     << GLES2Util::GetStringEnum(internalformat) << ")");
  helper_->FramebufferMemorylessPixelLocalStorageANGLE(plane, internalformat);
  CheckGLError();
}

void GLES2Implementation::FramebufferTexturePixelLocalStorageANGLE(
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferTexturePixelLocalStorageANGLE(" << plane
                     << ", " << backingtexture << ", " << level << ", " << layer
                     << ")");
  helper_->FramebufferTexturePixelLocalStorageANGLE(plane, backingtexture,
                                                    level, layer);
  CheckGLError();
}

void GLES2Implementation::FramebufferPixelLocalClearValuefvANGLE(
    GLint plane,
    const GLfloat* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferPixelLocalClearValuefvANGLE(" << plane
                     << ", " << static_cast<const void*>(value) << ")");
  uint32_t count = 4;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << value[ii]);
  helper_->FramebufferPixelLocalClearValuefvANGLEImmediate(plane, value);
  CheckGLError();
}

void GLES2Implementation::FramebufferPixelLocalClearValueivANGLE(
    GLint plane,
    const GLint* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferPixelLocalClearValueivANGLE(" << plane
                     << ", " << static_cast<const void*>(value) << ")");
  uint32_t count = 4;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << value[ii]);
  helper_->FramebufferPixelLocalClearValueivANGLEImmediate(plane, value);
  CheckGLError();
}

void GLES2Implementation::FramebufferPixelLocalClearValueuivANGLE(
    GLint plane,
    const GLuint* value) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferPixelLocalClearValueuivANGLE(" << plane
                     << ", " << static_cast<const void*>(value) << ")");
  uint32_t count =
      GLES2Util::CalcFramebufferPixelLocalClearValueuivANGLEDataCount(plane);
  DCHECK_LE(count, 4u);
  if (count == 0) {
    SetGLErrorInvalidEnum("glFramebufferPixelLocalClearValueuivANGLE", plane,
                          "plane");
    return;
  }
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << value[ii]);
  helper_->FramebufferPixelLocalClearValueuivANGLEImmediate(plane, value);
  CheckGLError();
}

void GLES2Implementation::BeginPixelLocalStorageANGLE(GLsizei count,
                                                      const GLenum* loadops) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBeginPixelLocalStorageANGLE("
                     << count << ", " << static_cast<const void*>(loadops)
                     << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << loadops[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glBeginPixelLocalStorageANGLE", "count < 0");
    return;
  }
  helper_->BeginPixelLocalStorageANGLEImmediate(count, loadops);
  CheckGLError();
}

void GLES2Implementation::EndPixelLocalStorageANGLE(GLsizei count,
                                                    const GLenum* storeops) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glEndPixelLocalStorageANGLE("
                     << count << ", " << static_cast<const void*>(storeops)
                     << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << storeops[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glEndPixelLocalStorageANGLE", "count < 0");
    return;
  }
  helper_->EndPixelLocalStorageANGLEImmediate(count, storeops);
  CheckGLError();
}

void GLES2Implementation::PixelLocalStorageBarrierANGLE() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glPixelLocalStorageBarrierANGLE("
                     << ")");
  helper_->PixelLocalStorageBarrierANGLE();
  CheckGLError();
}

void GLES2Implementation::FramebufferPixelLocalStorageInterruptANGLE() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferPixelLocalStorageInterruptANGLE("
                     << ")");
  helper_->FramebufferPixelLocalStorageInterruptANGLE();
  CheckGLError();
}

void GLES2Implementation::FramebufferPixelLocalStorageRestoreANGLE() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glFramebufferPixelLocalStorageRestoreANGLE("
                     << ")");
  helper_->FramebufferPixelLocalStorageRestoreANGLE();
  CheckGLError();
}

void GLES2Implementation::GetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint plane,
    GLenum pname,
    GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glGetFramebufferPixelLocalStorageParameterfvANGLE("
                     << plane << ", " << GLES2Util::GetStringEnum(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0(
      "gpu",
      "GLES2Implementation::GetFramebufferPixelLocalStorageParameterfvANGLE");
  if (GetFramebufferPixelLocalStorageParameterfvANGLEHelper(plane, pname,
                                                            params)) {
    return;
  }
  typedef cmds::GetFramebufferPixelLocalStorageParameterfvANGLE::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetFramebufferPixelLocalStorageParameterfvANGLE(
      plane, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void GLES2Implementation::GetFramebufferPixelLocalStorageParameterivANGLE(
    GLint plane,
    GLenum pname,
    GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glGetFramebufferPixelLocalStorageParameterivANGLE("
                     << plane << ", " << GLES2Util::GetStringEnum(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0(
      "gpu",
      "GLES2Implementation::GetFramebufferPixelLocalStorageParameterivANGLE");
  if (GetFramebufferPixelLocalStorageParameterivANGLEHelper(plane, pname,
                                                            params)) {
    return;
  }
  typedef cmds::GetFramebufferPixelLocalStorageParameterivANGLE::Result Result;
  ScopedResultPtr<Result> result = GetResultAs<Result>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetFramebufferPixelLocalStorageParameterivANGLE(
      plane, pname, GetResultShmId(), result.offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_IMPL_AUTOGEN_H_
