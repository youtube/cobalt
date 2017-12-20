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

#include <GLES2/gl2.h>

#include "glimp/gles/context.h"
#include "starboard/log.h"

namespace gles = glimp::gles;

namespace {
gles::Context* GetCurrentContext() {
  gles::Context* context = gles::Context::GetTLSCurrentContext();
  if (!context) {
    SB_DLOG(WARNING) << "GL ES command issued while no context was current.";
  }
  return context;
}
}

extern "C" {

void GL_APIENTRY glActiveTexture(GLenum texture) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->ActiveTexture(texture);
}

void GL_APIENTRY glAttachShader(GLuint program, GLuint shader) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->AttachShader(program, shader);
}

void GL_APIENTRY glBindAttribLocation(GLuint program,
                                      GLuint index,
                                      const GLchar* name) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BindAttribLocation(program, index, name);
}

void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BindBuffer(target, buffer);
}

void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BindFramebuffer(target, framebuffer);
}

void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BindRenderbuffer(target, renderbuffer);
}

void GL_APIENTRY glBindTexture(GLenum target, GLuint texture) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->BindTexture(target, texture);
}

void GL_APIENTRY glBlendColor(GLfloat red,
                              GLfloat green,
                              GLfloat blue,
                              GLfloat alpha) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBlendEquation(GLenum mode) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BlendEquation(mode);
}

void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BlendFunc(sfactor, dfactor);
}

void GL_APIENTRY glBlendFuncSeparate(GLenum srcRGB,
                                     GLenum dstRGB,
                                     GLenum srcAlpha,
                                     GLenum dstAlpha) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBufferData(GLenum target,
                              GLsizeiptr size,
                              const GLvoid* data,
                              GLenum usage) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BufferData(target, size, data, usage);
}

void GL_APIENTRY glBufferSubData(GLenum target,
                                 GLintptr offset,
                                 GLsizeiptr size,
                                 const GLvoid* data) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->BufferSubData(target, offset, size, data);
}

GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return 0;
  }

  return context->CheckFramebufferStatus(target);
}

void GL_APIENTRY glClear(GLbitfield mask) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Clear(mask);
}

void GL_APIENTRY glClearColor(GLfloat red,
                              GLfloat green,
                              GLfloat blue,
                              GLfloat alpha) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->ClearColor(red, green, blue, alpha);
}

void GL_APIENTRY glClearDepthf(GLfloat depth) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glClearStencil(GLint s) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->ClearStencil(s);
}

void GL_APIENTRY glColorMask(GLboolean red,
                             GLboolean green,
                             GLboolean blue,
                             GLboolean alpha) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->ColorMask(red, green, blue, alpha);
}

void GL_APIENTRY glCompileShader(GLuint shader) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->CompileShader(shader);
}

void GL_APIENTRY glCompressedTexImage2D(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        GLsizei imageSize,
                                        const GLvoid* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glCompressedTexSubImage2D(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const GLvoid* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glCopyTexImage2D(GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glCopyTexSubImage2D(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) {
  SB_NOTIMPLEMENTED();
}

GLuint GL_APIENTRY glCreateProgram(void) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return 0;
  }

  return context->CreateProgram();
}

GLuint GL_APIENTRY glCreateShader(GLenum type) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return 0;
  }

  return context->CreateShader(type);
}

void GL_APIENTRY glCullFace(GLenum mode) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->CullFace(mode);
}

void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint* buffers) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DeleteBuffers(n, buffers);
}

void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DeleteFramebuffers(n, framebuffers);
}

void GL_APIENTRY glDeleteProgram(GLuint program) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DeleteProgram(program);
}

void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DeleteRenderbuffers(n, renderbuffers);
}

void GL_APIENTRY glDeleteShader(GLuint shader) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DeleteShader(shader);
}

void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DeleteTextures(n, textures);
}

void GL_APIENTRY glDepthFunc(GLenum func) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDepthMask(GLboolean flag) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->DepthMask(flag);
}

void GL_APIENTRY glDepthRangef(GLfloat n, GLfloat f) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDetachShader(GLuint program, GLuint shader) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDisable(GLenum cap) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Disable(cap);
}

void GL_APIENTRY glDisableVertexAttribArray(GLuint index) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DisableVertexAttribArray(index);
}

void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DrawArrays(mode, first, count);
}

void GL_APIENTRY glDrawElements(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const GLvoid* indices) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->DrawElements(mode, count, type, indices);
}

void GL_APIENTRY glEnable(GLenum cap) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Enable(cap);
}

void GL_APIENTRY glEnableVertexAttribArray(GLuint index) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->EnableVertexAttribArray(index);
}

void GL_APIENTRY glFinish(void) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->Finish();
}

void GL_APIENTRY glFlush(void) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->Flush();
}

void GL_APIENTRY glFramebufferRenderbuffer(GLenum target,
                                           GLenum attachment,
                                           GLenum renderbuffertarget,
                                           GLuint renderbuffer) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->FramebufferRenderbuffer(target, attachment,
                                          renderbuffertarget, renderbuffer);
}

void GL_APIENTRY glFramebufferTexture2D(GLenum target,
                                        GLenum attachment,
                                        GLenum textarget,
                                        GLuint texture,
                                        GLint level) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->FramebufferTexture2D(target, attachment, textarget, texture,
                                       level);
}

void GL_APIENTRY glFrontFace(GLenum face) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->FrontFace(face);
}

void GL_APIENTRY glGenBuffers(GLsizei n, GLuint* buffers) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->GenBuffers(n, buffers);
}

void GL_APIENTRY glGenerateMipmap(GLenum target) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->GenFramebuffers(n, framebuffers);
}

void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->GenRenderbuffers(n, renderbuffers);
}

void GL_APIENTRY glGenTextures(GLsizei n, GLuint* textures) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->GenTextures(n, textures);
}

void GL_APIENTRY glGetActiveAttrib(GLuint program,
                                   GLuint index,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   GLint* size,
                                   GLenum* type,
                                   GLchar* name) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetActiveUniform(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    GLchar* name) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetAttachedShaders(GLuint program,
                                      GLsizei maxcount,
                                      GLsizei* count,
                                      GLuint* shaders) {
  SB_NOTIMPLEMENTED();
}

GLint GL_APIENTRY glGetAttribLocation(GLuint program, const GLchar* name) {
  SB_NOTIMPLEMENTED();
  return 0;
}

void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetBufferParameteriv(GLenum target,
                                        GLenum pname,
                                        GLint* params) {
  SB_NOTIMPLEMENTED();
}

GLenum GL_APIENTRY glGetError(void) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return GL_NO_ERROR;
  }

  return context->GetError();
}

void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target,
                                                       GLenum attachment,
                                                       GLenum pname,
                                                       GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetIntegerv(GLenum pname, GLint* params) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->GetIntegerv(pname, params);
}

void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->GetProgramiv(program, pname, params);
}

void GL_APIENTRY glGetProgramInfoLog(GLuint program,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLchar* infolog) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->GetProgramInfoLog(program, bufsize, length, infolog);
}

void GL_APIENTRY glGetRenderbufferParameteriv(GLenum target,
                                              GLenum pname,
                                              GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->GetShaderiv(shader, pname, params);
}

void GL_APIENTRY glGetShaderInfoLog(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLchar* infolog) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GL_APIENTRY glGetShaderPrecisionFormat(GLenum shadertype,
                                            GLenum precisiontype,
                                            GLint* range,
                                            GLint* precision) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetShaderSource(GLuint shader,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   GLchar* source) {
  SB_NOTIMPLEMENTED();
}

const GLubyte* GL_APIENTRY glGetString(GLenum name) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return NULL;
  }

  return context->GetString(name);
}

void GL_APIENTRY glGetTexParameterfv(GLenum target,
                                     GLenum pname,
                                     GLfloat* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetTexParameteriv(GLenum target,
                                     GLenum pname,
                                     GLint* params) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->GetTexParameteriv(target, pname, params);
}

void GL_APIENTRY glGetUniformfv(GLuint program,
                                GLint location,
                                GLfloat* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetUniformiv(GLuint program, GLint location, GLint* params) {
  SB_NOTIMPLEMENTED();
}

GLint GL_APIENTRY glGetUniformLocation(GLuint program, const GLchar* name) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return -1;
  }

  return context->GetUniformLocation(program, name);
}

void GL_APIENTRY glGetVertexAttribfv(GLuint index,
                                     GLenum pname,
                                     GLfloat* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetVertexAttribiv(GLuint index,
                                     GLenum pname,
                                     GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetVertexAttribPointerv(GLuint index,
                                           GLenum pname,
                                           GLvoid** pointer) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glHint(GLenum target, GLenum mode) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsBuffer(GLuint buffer) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glIsEnabled(GLenum cap) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glIsFramebuffer(GLuint framebuffer) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glIsProgram(GLuint program) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glIsRenderbuffer(GLuint renderbuffer) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glIsShader(GLuint shader) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glIsTexture(GLuint texture) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glLineWidth(GLfloat width) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->LineWidth(width);
}

void GL_APIENTRY glLinkProgram(GLuint program) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->LinkProgram(program);
}

void GL_APIENTRY glPixelStorei(GLenum pname, GLint param) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->PixelStorei(pname, param);
}

void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glReadPixels(GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLenum format,
                              GLenum type,
                              GLvoid* pixels) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->ReadPixels(x, y, width, height, format, type, pixels);
}

void GL_APIENTRY glReleaseShaderCompiler(void) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glRenderbufferStorage(GLenum target,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->RenderbufferStorage(target, internalformat, width, height);
}

void GL_APIENTRY glSampleCoverage(GLfloat value, GLboolean invert) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->Scissor(x, y, width, height);
}

void GL_APIENTRY glShaderBinary(GLsizei n,
                                const GLuint* shaders,
                                GLenum binaryformat,
                                const GLvoid* binary,
                                GLsizei length) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glShaderSource(GLuint shader,
                                GLsizei count,
                                const GLchar* const* string,
                                const GLint* length) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->ShaderSource(shader, count, string, length);
}

void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glStencilFuncSeparate(GLenum face,
                                       GLenum func,
                                       GLint ref,
                                       GLuint mask) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glStencilMask(GLuint mask) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->StencilMask(mask);
}

void GL_APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glStencilOpSeparate(GLenum face,
                                     GLenum fail,
                                     GLenum zfail,
                                     GLenum zpass) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexImage2D(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const GLvoid* pixels) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->TexImage2D(target, level, internalformat, width, height,
                             border, format, type, pixels);
}

void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexParameterfv(GLenum target,
                                  GLenum pname,
                                  const GLfloat* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->TexParameteri(target, pname, param);
}

void GL_APIENTRY glTexParameteriv(GLenum target,
                                  GLenum pname,
                                  const GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 const GLvoid* pixels) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                                format, type, pixels);
}

void GL_APIENTRY glUniform1f(GLint location, GLfloat x) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformfv(location, 1, 1, &x);
}

void GL_APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformfv(location, count, 1, v);
}

void GL_APIENTRY glUniform1i(GLint location, GLint x) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformiv(location, 1, 1, &x);
}

void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformiv(location, count, 1, v);
}

void GL_APIENTRY glUniform2f(GLint location, GLfloat x, GLfloat y) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  float v[2];
  v[0] = x;
  v[1] = y;
  context->Uniformfv(location, 1, 2, v);
}

void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformfv(location, count, 2, v);
}

void GL_APIENTRY glUniform2i(GLint location, GLint x, GLint y) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  int v[2];
  v[0] = x;
  v[1] = y;
  context->Uniformiv(location, 1, 2, v);
}

void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformiv(location, count, 2, v);
}

void GL_APIENTRY glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  float v[3];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  context->Uniformfv(location, 1, 3, v);
}

void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformfv(location, count, 3, v);
}

void GL_APIENTRY glUniform3i(GLint location, GLint x, GLint y, GLint z) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  int v[3];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  context->Uniformiv(location, 1, 3, v);
}

void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformiv(location, count, 3, v);
}

void GL_APIENTRY
glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  float v[4];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  context->Uniformfv(location, 1, 4, v);
}

void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformfv(location, count, 4, v);
}

void GL_APIENTRY
glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  int v[4];
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
  context->Uniformiv(location, 1, 4, v);
}

void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint* v) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->Uniformiv(location, count, 4, v);
}

void GL_APIENTRY glUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->UniformMatrixfv(location, count, transpose, 2, value);
}

void GL_APIENTRY glUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->UniformMatrixfv(location, count, transpose, 3, value);
}

void GL_APIENTRY glUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  context->UniformMatrixfv(location, count, transpose, 4, value);
}

void GL_APIENTRY glUseProgram(GLuint program) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->UseProgram(program);
}

void GL_APIENTRY glValidateProgram(GLuint program) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glVertexAttrib1f(GLuint indx, GLfloat x) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->VertexAttribfv(indx, 1, &x);
}

void GL_APIENTRY glVertexAttrib1fv(GLuint indx, const GLfloat* values) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->VertexAttribfv(indx, 1, values);
}

void GL_APIENTRY glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  GLfloat values[2] = {x, y};
  return context->VertexAttribfv(indx, 2, values);
}

void GL_APIENTRY glVertexAttrib2fv(GLuint indx, const GLfloat* values) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->VertexAttribfv(indx, 2, values);
}

void GL_APIENTRY glVertexAttrib3f(GLuint indx,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  GLfloat values[3] = {x, y, z};
  return context->VertexAttribfv(indx, 3, values);
}

void GL_APIENTRY glVertexAttrib3fv(GLuint indx, const GLfloat* values) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->VertexAttribfv(indx, 3, values);
}

void GL_APIENTRY
glVertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  GLfloat values[4] = {x, y, z, w};
  return context->VertexAttribfv(indx, 4, values);
}

void GL_APIENTRY glVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->VertexAttribfv(indx, 4, values);
}

void GL_APIENTRY glVertexAttribPointer(GLuint indx,
                                       GLint size,
                                       GLenum type,
                                       GLboolean normalized,
                                       GLsizei stride,
                                       const GLvoid* ptr) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->VertexAttribPointer(indx, size, type, normalized, stride,
                                      ptr);
}

void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return;
  }

  return context->Viewport(x, y, width, height);
}

}  // extern "C"
