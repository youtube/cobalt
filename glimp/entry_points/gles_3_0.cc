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

#include <GLES3/gl3.h>

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
}  // namespace

extern "C" {

void GL_APIENTRY glReadBuffer(GLenum mode) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawRangeElements(GLenum mode,
                                     GLuint start,
                                     GLuint end,
                                     GLsizei count,
                                     GLenum type,
                                     const GLvoid* indices) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexImage3D(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const GLvoid* pixels) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexSubImage3D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 const GLvoid* pixels) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glCopyTexSubImage3D(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glCompressedTexImage3D(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLsizei imageSize,
                                        const GLvoid* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glCompressedTexSubImage3D(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const GLvoid* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenQueries(GLsizei n, GLuint* ids) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteQueries(GLsizei n, const GLuint* ids) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsQuery(GLuint id) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glBeginQuery(GLenum target, GLuint id) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glEndQuery(GLenum target) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetQueryiv(GLenum target, GLenum pname, GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glUnmapBuffer(GLenum target) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return GL_FALSE;
  }

  return context->UnmapBuffer(target);
}

void GL_APIENTRY glGetBufferPointerv(GLenum target,
                                     GLenum pname,
                                     GLvoid** params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawBuffers(GLsizei n, const GLenum* bufs) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformMatrix2x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformMatrix3x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformMatrix2x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformMatrix4x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformMatrix3x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformMatrix4x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBlitFramebuffer(GLint srcX0,
                                   GLint srcY0,
                                   GLint srcX1,
                                   GLint srcY1,
                                   GLint dstX0,
                                   GLint dstY0,
                                   GLint dstX1,
                                   GLint dstY1,
                                   GLbitfield mask,
                                   GLenum filter) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glRenderbufferStorageMultisample(GLenum target,
                                                  GLsizei samples,
                                                  GLenum internalformat,
                                                  GLsizei width,
                                                  GLsizei height) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glFramebufferTextureLayer(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLint layer) {
  SB_NOTIMPLEMENTED();
}

GLvoid* GL_APIENTRY glMapBufferRange(GLenum target,
                                     GLintptr offset,
                                     GLsizeiptr length,
                                     GLbitfield access) {
  gles::Context* context = GetCurrentContext();
  if (!context) {
    return NULL;
  }

  return context->MapBufferRange(target, offset, length, access);
}

void GL_APIENTRY glFlushMappedBufferRange(GLenum target,
                                          GLintptr offset,
                                          GLsizeiptr length) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBindVertexArray(GLuint array) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenVertexArrays(GLsizei n, GLuint* arrays) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsVertexArray(GLuint array) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glGetIntegeri_v(GLenum target, GLuint index, GLint* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBeginTransformFeedback(GLenum primitiveMode) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glEndTransformFeedback(void) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBindBufferRange(GLenum target,
                                   GLuint index,
                                   GLuint buffer,
                                   GLintptr offset,
                                   GLsizeiptr size) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTransformFeedbackVaryings(GLuint program,
                                             GLsizei count,
                                             const GLchar* const* varyings,
                                             GLenum bufferMode) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetTransformFeedbackVarying(GLuint program,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLsizei* size,
                                               GLenum* type,
                                               GLchar* name) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glVertexAttribIPointer(GLuint index,
                                        GLint size,
                                        GLenum type,
                                        GLsizei stride,
                                        const GLvoid* pointer) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetVertexAttribIiv(GLuint index,
                                      GLenum pname,
                                      GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetVertexAttribIuiv(GLuint index,
                                       GLenum pname,
                                       GLuint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY
glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY
glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glVertexAttribI4iv(GLuint index, const GLint* v) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glVertexAttribI4uiv(GLuint index, const GLuint* v) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetUniformuiv(GLuint program,
                                 GLint location,
                                 GLuint* params) {
  SB_NOTIMPLEMENTED();
}

GLint GL_APIENTRY glGetFragDataLocation(GLuint program, const GLchar* name) {
  SB_NOTIMPLEMENTED();
  return 0;
}

void GL_APIENTRY glUniform1ui(GLint location, GLuint v0) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniform2ui(GLint location, GLuint v0, GLuint v1) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY
glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniform1uiv(GLint location,
                               GLsizei count,
                               const GLuint* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniform2uiv(GLint location,
                               GLsizei count,
                               const GLuint* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniform3uiv(GLint location,
                               GLsizei count,
                               const GLuint* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniform4uiv(GLint location,
                               GLsizei count,
                               const GLuint* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glClearBufferiv(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLint* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glClearBufferuiv(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLuint* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glClearBufferfv(GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat* value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glClearBufferfi(GLenum buffer,
                                 GLint drawbuffer,
                                 GLfloat depth,
                                 GLint stencil) {
  SB_NOTIMPLEMENTED();
}

const GLubyte* GL_APIENTRY glGetStringi(GLenum name, GLuint index) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

void GL_APIENTRY glCopyBufferSubData(GLenum readTarget,
                                     GLenum writeTarget,
                                     GLintptr readOffset,
                                     GLintptr writeOffset,
                                     GLsizeiptr size) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetUniformIndices(GLuint program,
                                     GLsizei uniformCount,
                                     const GLchar* const* uniformNames,
                                     GLuint* uniformIndices) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetActiveUniformsiv(GLuint program,
                                       GLsizei uniformCount,
                                       const GLuint* uniformIndices,
                                       GLenum pname,
                                       GLint* params) {
  SB_NOTIMPLEMENTED();
}

GLuint GL_APIENTRY glGetUniformBlockIndex(GLuint program,
                                          const GLchar* uniformBlockName) {
  SB_NOTIMPLEMENTED();
  return 0;
}

void GL_APIENTRY glGetActiveUniformBlockiv(GLuint program,
                                           GLuint uniformBlockIndex,
                                           GLenum pname,
                                           GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetActiveUniformBlockName(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLchar* uniformBlockName) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glUniformBlockBinding(GLuint program,
                                       GLuint uniformBlockIndex,
                                       GLuint uniformBlockBinding) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawArraysInstanced(GLenum mode,
                                       GLint first,
                                       GLsizei count,
                                       GLsizei instanceCount) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawElementsInstanced(GLenum mode,
                                         GLsizei count,
                                         GLenum type,
                                         const GLvoid* indices,
                                         GLsizei instanceCount) {
  SB_NOTIMPLEMENTED();
}

GLsync GL_APIENTRY glFenceSync(GLenum condition, GLbitfield flags) {
  SB_NOTIMPLEMENTED();
  return 0;
}

GLboolean GL_APIENTRY glIsSync(GLsync sync) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glDeleteSync(GLsync sync) {
  SB_NOTIMPLEMENTED();
}

GLenum GL_APIENTRY glClientWaitSync(GLsync sync,
                                    GLbitfield flags,
                                    GLuint64 timeout) {
  SB_NOTIMPLEMENTED();
  return 0;
}

void GL_APIENTRY glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetInteger64v(GLenum pname, GLint64* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetSynciv(GLsync sync,
                             GLenum pname,
                             GLsizei bufSize,
                             GLsizei* length,
                             GLint* values) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetInteger64i_v(GLenum target, GLuint index, GLint64* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetBufferParameteri64v(GLenum target,
                                          GLenum pname,
                                          GLint64* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenSamplers(GLsizei count, GLuint* samplers) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteSamplers(GLsizei count, const GLuint* samplers) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsSampler(GLuint sampler) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glBindSampler(GLuint unit, GLuint sampler) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glSamplerParameteri(GLuint sampler,
                                     GLenum pname,
                                     GLint param) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glSamplerParameteriv(GLuint sampler,
                                      GLenum pname,
                                      const GLint* param) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glSamplerParameterf(GLuint sampler,
                                     GLenum pname,
                                     GLfloat param) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glSamplerParameterfv(GLuint sampler,
                                      GLenum pname,
                                      const GLfloat* param) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetSamplerParameteriv(GLuint sampler,
                                         GLenum pname,
                                         GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetSamplerParameterfv(GLuint sampler,
                                         GLenum pname,
                                         GLfloat* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glVertexAttribDivisor(GLuint index, GLuint divisor) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBindTransformFeedback(GLenum target, GLuint id) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenTransformFeedbacks(GLsizei n, GLuint* ids) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsTransformFeedback(GLuint id) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glPauseTransformFeedback(void) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glResumeTransformFeedback(void) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetProgramBinary(GLuint program,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    GLenum* binaryFormat,
                                    GLvoid* binary) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glProgramBinary(GLuint program,
                                 GLenum binaryFormat,
                                 const GLvoid* binary,
                                 GLsizei length) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glProgramParameteri(GLuint program,
                                     GLenum pname,
                                     GLint value) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glInvalidateFramebuffer(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum* attachments) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glInvalidateSubFramebuffer(GLenum target,
                                            GLsizei numAttachments,
                                            const GLenum* attachments,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexStorage2D(GLenum target,
                                GLsizei levels,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexStorage3D(GLenum target,
                                GLsizei levels,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetInternalformativ(GLenum target,
                                       GLenum internalformat,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLint* params) {
  SB_NOTIMPLEMENTED();
}

}  // extern "C"
