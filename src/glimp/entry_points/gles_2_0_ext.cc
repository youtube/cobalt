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
#include <GLES2/gl2ext.h>

#include "starboard/common/log.h"

extern "C" {

void GL_APIENTRY glBlitFramebufferANGLE(GLint srcX0,
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

void GL_APIENTRY glRenderbufferStorageMultisampleANGLE(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDiscardFramebufferEXT(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum* attachments) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteFencesNV(GLsizei n, const GLuint* fences) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenFencesNV(GLsizei n, GLuint* fences) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsFenceNV(GLuint fence) {
  SB_NOTIMPLEMENTED();
  return false;
}

GLboolean GL_APIENTRY glTestFenceNV(GLuint fence) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glGetFenceivNV(GLuint fence, GLenum pname, GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glFinishFenceNV(GLuint fence) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glSetFenceNV(GLuint fence, GLenum condition) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetTranslatedShaderSourceANGLE(GLuint shader,
                                                  GLsizei bufsize,
                                                  GLsizei* length,
                                                  GLchar* source) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glTexStorage2DEXT(GLenum target,
                                   GLsizei levels,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height) {
  SB_NOTIMPLEMENTED();
}

GLenum GL_APIENTRY glGetGraphicsResetStatusEXT(void) {
  SB_NOTIMPLEMENTED();
  return 0;
}

void GL_APIENTRY glReadnPixelsEXT(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  void* data) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetnUniformfvEXT(GLuint program,
                                    GLint location,
                                    GLsizei bufSize,
                                    float* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetnUniformivEXT(GLuint program,
                                    GLint location,
                                    GLsizei bufSize,
                                    GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenQueriesEXT(GLsizei n, GLuint* ids) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteQueriesEXT(GLsizei n, const GLuint* ids) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsQueryEXT(GLuint id) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glBeginQueryEXT(GLenum target, GLuint id) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glEndQueryEXT(GLenum target) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetQueryivEXT(GLenum target, GLenum pname, GLint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetQueryObjectuivEXT(GLuint id,
                                        GLenum pname,
                                        GLuint* params) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawBuffersEXT(GLsizei n, const GLenum* bufs) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawArraysInstancedANGLE(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDrawElementsInstancedANGLE(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices,
                                              GLsizei primcount) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glVertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGetProgramBinaryOES(GLuint program,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLenum* binaryFormat,
                                       GLvoid* binary) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glProgramBinaryOES(GLuint program,
                                    GLenum binaryFormat,
                                    const GLvoid* binary,
                                    GLint length) {
  SB_NOTIMPLEMENTED();
}

void* GL_APIENTRY glMapBufferOES(GLenum target, GLenum access) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

GLboolean GL_APIENTRY glUnmapBufferOES(GLenum target) {
  SB_NOTIMPLEMENTED();
  return false;
}

void GL_APIENTRY glGetBufferPointervOES(GLenum target,
                                        GLenum pname,
                                        GLvoid** params) {
  SB_NOTIMPLEMENTED();
}

void* GL_APIENTRY glMapBufferRangeEXT(GLenum target,
                                      GLintptr offset,
                                      GLsizeiptr length,
                                      GLbitfield access) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

void GL_APIENTRY glFlushMappedBufferRangeEXT(GLenum target,
                                             GLintptr offset,
                                             GLsizeiptr length) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glInsertEventMarkerEXT(GLsizei length, const char* marker) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glPushGroupMarkerEXT(GLsizei length, const char* marker) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glPopGroupMarkerEXT() {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glEGLImageTargetTexture2DOES(GLenum target,
                                              GLeglImageOES image) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glEGLImageTargetRenderbufferStorageOES(GLenum target,
                                                        GLeglImageOES image) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glBindVertexArrayOES(GLuint array) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n, const GLuint* arrays) {
  SB_NOTIMPLEMENTED();
}

void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint* arrays) {
  SB_NOTIMPLEMENTED();
}

GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint array) {
  SB_NOTIMPLEMENTED();
  return false;
}

}  // extern "C"
