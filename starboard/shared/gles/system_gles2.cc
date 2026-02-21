// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <map>
#include <tuple>
#include "starboard/common/log.h"
#include "starboard/gles.h"

#define GL_MEM_TRACE(x) trace_##x

enum Objects { Buffers = 0, Textures = 1, Renderbuffers = 2 };

const char* type2str(Objects type) {
  switch (type) {
    case Buffers:
      return "buffers";
    case Textures:
      return "textures";
    case Renderbuffers:
      return "renderbuffers";
    default:
      SB_NOTREACHED();
  }
}

using MemObjects = std::map<GLuint, size_t>;
struct TrackedMemObject {
  GLuint active = 0;
  MemObjects objects;
  size_t total_allocation = 0;
};

TrackedMemObject* getObjects() {
  static TrackedMemObject objects[3] = {
      TrackedMemObject(),
      TrackedMemObject(),
      TrackedMemObject(),
  };
  return objects;
}

void genObjects(Objects type, GLsizei n, GLuint* objects) {
  TrackedMemObject& tracked_object = getObjects()[type];
  MemObjects& map = tracked_object.objects;
  for (GLsizei i = 0; i < n; i++) {
    GLuint object_id = objects[i];
    auto existing = map.find(object_id);
    SB_CHECK(existing == map.end());
    map.insert(std::make_pair(object_id, 0));
    SB_LOG(ERROR) << "GLTRACE: added type:" << type2str(type)
                  << " object:" << object_id << " p:" << &tracked_object;
  }
}
void deleteObjects(Objects type, GLsizei n, const GLuint* objects) {
  TrackedMemObject& tracked_object = getObjects()[type];
  MemObjects& map = tracked_object.objects;
  for (GLsizei i = 0; i < n; i++) {
    GLuint object_id = objects[i];
    auto existing = map.find(object_id);
    SB_CHECK(existing != map.end());
    if (existing->second != 0) {
      SB_LOG(ERROR) << "GLTRACE: Released alloc for type:" << type2str(type)
                    << " size: " << existing->second
                    << " total:" << tracked_object.total_allocation << " (MB:"
                    << (tracked_object.total_allocation / (1024 * 1024)) << ")";
    }
    tracked_object.total_allocation -= existing->second;
    map.erase(existing);
  }
}
void bindObject(Objects type, GLuint object) {
  TrackedMemObject& tracked_object = getObjects()[type];
  MemObjects& map = tracked_object.objects;
  tracked_object.active = object;
  if (object == 0)
    return;
  auto existing = map.find(object);
  if (existing == map.end()) {
    SB_LOG(ERROR) << "GLTRACE:  Cannot find object to bind, type:"
                  << type2str(type) << " object:" << object;
    SB_CHECK(existing != map.end());
  }
}
void reportAllocation(Objects type, size_t estimated_allocation) {
  TrackedMemObject& tracked_object = getObjects()[type];
  MemObjects& map = tracked_object.objects;
  auto existing = map.find(tracked_object.active);
  SB_CHECK(existing != map.end());
  // First subtract the previous allocation
  tracked_object.total_allocation -= existing->second;
  // update and add
  existing->second = estimated_allocation;
  tracked_object.total_allocation += existing->second;
  SB_LOG(ERROR) << "GLTRACE: Alloc for type:" << type2str(type)
                << " size: " << estimated_allocation
                << " total:" << tracked_object.total_allocation
                << " (MB:" << (tracked_object.total_allocation / (1024 * 1024))
                << ")";
}

// Buffers
void GL_MEM_TRACE(glGenBuffers)(GLsizei n, GLuint* buffers) {
  glGenBuffers(n, buffers);
  genObjects(Buffers, n, buffers);
}
void GL_MEM_TRACE(glDeleteBuffers)(GLsizei n, const GLuint* buffers) {
  glDeleteBuffers(n, buffers);
  deleteObjects(Buffers, n, buffers);
}
void GL_MEM_TRACE(glBindBuffer)(GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
  bindObject(Buffers, buffer);
}
void GL_MEM_TRACE(glBufferData)(GLenum target,
                                GLsizeiptr size,
                                const void* data,
                                GLenum usage) {
  glBufferData(target, size, data, usage);
  reportAllocation(Buffers, size);
}

// Textures
void GL_MEM_TRACE(glGenTextures)(GLsizei n, GLuint* textures) {
  glGenTextures(n, textures);
  genObjects(Textures, n, textures);
}
void GL_MEM_TRACE(glDeleteTextures)(GLsizei n, const GLuint* textures) {
  glDeleteTextures(n, textures);
  deleteObjects(Textures, n, textures);
}
void GL_MEM_TRACE(glBindTexture)(GLenum target, GLuint texture) {
  glBindTexture(target, texture);
  bindObject(Textures, texture);
}

size_t estimate_bytes_per_pixel(GLenum format) {
  switch (format) {
    case GL_RGB:
      return 3;
    case GL_RGBA:
      return 4;
    case GL_LUMINANCE:
    case GL_ALPHA:
      return 1;
    default:
      return 4;  // overcount
  }
}

void GL_MEM_TRACE(glTexImage2D)(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  glTexImage2D(target, level, internalformat, width, height, border, format,
               type, pixels);
  reportAllocation(Textures, width * height * estimate_bytes_per_pixel(format));
}
void GL_MEM_TRACE(glTexSubImage2D)(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
                  pixels);
  reportAllocation(Textures, width * height * estimate_bytes_per_pixel(format));
}

// Renderbuffers
void GL_MEM_TRACE(glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers) {
  glGenRenderbuffers(n, renderbuffers);
  genObjects(Renderbuffers, n, renderbuffers);
}
void GL_MEM_TRACE(glDeleteRenderbuffers)(GLsizei n,
                                         const GLuint* renderbuffers) {
  glDeleteRenderbuffers(n, renderbuffers);
  deleteObjects(Renderbuffers, n, renderbuffers);
}
void GL_MEM_TRACE(glBindRenderbuffer)(GLenum target, GLuint renderbuffer) {
  glBindRenderbuffer(target, renderbuffer);
  bindObject(Renderbuffers, renderbuffer);
}
void GL_MEM_TRACE(glRenderbufferStorage)(GLenum target,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height) {
  glRenderbufferStorage(target, internalformat, width, height);
  reportAllocation(Renderbuffers, width * height * 4);
}

namespace {

const SbGlesInterface g_sb_gles_interface = {
    &glActiveTexture,
    &glAttachShader,
    &glBindAttribLocation,
    &GL_MEM_TRACE(glBindBuffer),
    &glBindFramebuffer,
    &GL_MEM_TRACE(glBindRenderbuffer),
    &GL_MEM_TRACE(glBindTexture),
    &glBlendColor,
    &glBlendEquation,
    &glBlendEquationSeparate,
    &glBlendFunc,
    &glBlendFuncSeparate,
    &GL_MEM_TRACE(glBufferData),
    &glBufferSubData,
    &glCheckFramebufferStatus,
    &glClear,
    &glClearColor,
    &glClearDepthf,
    &glClearStencil,
    &glColorMask,
    &glCompileShader,
    &glCompressedTexImage2D,
    &glCompressedTexSubImage2D,
    &glCopyTexImage2D,
    &glCopyTexSubImage2D,
    &glCreateProgram,
    &glCreateShader,
    &glCullFace,
    &GL_MEM_TRACE(glDeleteBuffers),
    &glDeleteFramebuffers,
    &glDeleteProgram,
    &GL_MEM_TRACE(glDeleteRenderbuffers),
    &glDeleteShader,
    &GL_MEM_TRACE(glDeleteTextures),
    &glDepthFunc,
    &glDepthMask,
    &glDepthRangef,
    &glDetachShader,
    &glDisable,
    &glDisableVertexAttribArray,
    &glDrawArrays,
    &glDrawElements,
    &glEnable,
    &glEnableVertexAttribArray,
    &glFinish,
    &glFlush,
    &glFramebufferRenderbuffer,
    &glFramebufferTexture2D,
    &glFrontFace,
    &GL_MEM_TRACE(glGenBuffers),
    &glGenerateMipmap,
    &glGenFramebuffers,
    &GL_MEM_TRACE(glGenRenderbuffers),
    &GL_MEM_TRACE(glGenTextures),
    &glGetActiveAttrib,
    &glGetActiveUniform,
    &glGetAttachedShaders,
    &glGetAttribLocation,
    &glGetBooleanv,
    &glGetBufferParameteriv,
    &glGetError,
    &glGetFloatv,
    &glGetFramebufferAttachmentParameteriv,
    &glGetIntegerv,
    &glGetProgramiv,
    &glGetProgramInfoLog,
    &glGetRenderbufferParameteriv,
    &glGetShaderiv,
    &glGetShaderInfoLog,
    &glGetShaderPrecisionFormat,
    &glGetShaderSource,
    &glGetString,
    &glGetTexParameterfv,
    &glGetTexParameteriv,
    &glGetUniformfv,
    &glGetUniformiv,
    &glGetUniformLocation,
    &glGetVertexAttribfv,
    &glGetVertexAttribiv,
    &glGetVertexAttribPointerv,
    &glHint,
    &glIsBuffer,
    &glIsEnabled,
    &glIsFramebuffer,
    &glIsProgram,
    &glIsRenderbuffer,
    &glIsShader,
    &glIsTexture,
    &glLineWidth,
    &glLinkProgram,
    &glPixelStorei,
    &glPolygonOffset,
    &glReadPixels,
    &glReleaseShaderCompiler,
    &GL_MEM_TRACE(glRenderbufferStorage),
    &glSampleCoverage,
    &glScissor,
    &glShaderBinary,
    &glShaderSource,
    &glStencilFunc,
    &glStencilFuncSeparate,
    &glStencilMask,
    &glStencilMaskSeparate,
    &glStencilOp,
    &glStencilOpSeparate,
    &GL_MEM_TRACE(glTexImage2D),
    &glTexParameterf,
    &glTexParameterfv,
    &glTexParameteri,
    &glTexParameteriv,
    &GL_MEM_TRACE(glTexSubImage2D),
    &glUniform1f,
    &glUniform1fv,
    &glUniform1i,
    &glUniform1iv,
    &glUniform2f,
    &glUniform2fv,
    &glUniform2i,
    &glUniform2iv,
    &glUniform3f,
    &glUniform3fv,
    &glUniform3i,
    &glUniform3iv,
    &glUniform4f,
    &glUniform4fv,
    &glUniform4i,
    &glUniform4iv,
    &glUniformMatrix2fv,
    &glUniformMatrix3fv,
    &glUniformMatrix4fv,
    &glUseProgram,
    &glValidateProgram,
    &glVertexAttrib1f,
    &glVertexAttrib1fv,
    &glVertexAttrib2f,
    &glVertexAttrib2fv,
    &glVertexAttrib3f,
    &glVertexAttrib3fv,
    &glVertexAttrib4f,
    &glVertexAttrib4fv,
    &glVertexAttribPointer,
    &glViewport,
    nullptr,  // glReadBuffer
    nullptr,  // glDrawRangeElements
    nullptr,  // glTexImage3D
    nullptr,  // glTexSubImage3D
    nullptr,  // glCopyTexSubImage3D
    nullptr,  // glCompressedTexImage3D
    nullptr,  // glCompressedTexSubImage3D
    nullptr,  // glGenQueries
    nullptr,  // glDeleteQueries
    nullptr,  // glIsQuery
    nullptr,  // glBeginQuery
    nullptr,  // glEndQuery
    nullptr,  // glGetQueryiv
    nullptr,  // glGetQueryObjectuiv
    nullptr,  // glUnmapBuffer
    nullptr,  // glGetBufferPointerv
    nullptr,  // glDrawBuffers
    nullptr,  // glUniformMatrix2x3fv
    nullptr,  // glUniformMatrix3x2fv
    nullptr,  // glUniformMatrix2x4fv
    nullptr,  // glUniformMatrix4x2fv
    nullptr,  // glUniformMatrix3x4fv
    nullptr,  // glUniformMatrix4x3fv
    nullptr,  // glBlitFramebuffer
    nullptr,  // glRenderbufferStorageMultisample
    nullptr,  // glFramebufferTextureLayer
    nullptr,  // glMapBufferRange
    nullptr,  // glFlushMappedBufferRange
    nullptr,  // glBindVertexArray
    nullptr,  // glDeleteVertexArrays
    nullptr,  // glGenVertexArrays
    nullptr,  // glIsVertexArray
    nullptr,  // glGetIntegeri_v
    nullptr,  // glBeginTransformFeedback
    nullptr,  // glEndTransformFeedback
    nullptr,  // glBindBufferRange
    nullptr,  // glBindBufferBase
    nullptr,  // glTransformFeedbackVaryings
    nullptr,  // glGetTransformFeedbackVarying
    nullptr,  // glVertexAttribIPointer
    nullptr,  // glGetVertexAttribIiv
    nullptr,  // glGetVertexAttribIuiv
    nullptr,  // glVertexAttribI4i
    nullptr,  // glVertexAttribI4ui
    nullptr,  // glVertexAttribI4iv
    nullptr,  // glVertexAttribI4uiv
    nullptr,  // glGetUniformuiv
    nullptr,  // glGetFragDataLocation
    nullptr,  // glUniform1ui
    nullptr,  // glUniform2ui
    nullptr,  // glUniform3ui
    nullptr,  // glUniform4ui
    nullptr,  // glUniform1uiv
    nullptr,  // glUniform2uiv
    nullptr,  // glUniform3uiv
    nullptr,  // glUniform4uiv
    nullptr,  // glClearBufferiv
    nullptr,  // glClearBufferuiv
    nullptr,  // glClearBufferfv
    nullptr,  // glClearBufferfi
    nullptr,  // glGetStringi
    nullptr,  // glCopyBufferSubData
    nullptr,  // glGetUniformIndices
    nullptr,  // glGetActiveUniformsiv
    nullptr,  // glGetUniformBlockIndex
    nullptr,  // glGetActiveUniformBlockiv
    nullptr,  // glGetActiveUniformBlockName
    nullptr,  // glUniformBlockBinding
    nullptr,  // glDrawArraysInstanced
    nullptr,  // glDrawElementsInstanced
    nullptr,  // glFenceSync
    nullptr,  // glIsSync
    nullptr,  // glDeleteSync
    nullptr,  // glClientWaitSync
    nullptr,  // glWaitSync
    nullptr,  // glGetInteger64v
    nullptr,  // glGetSynciv
    nullptr,  // glGetInteger64i_v
    nullptr,  // glGetBufferParameteri64v
    nullptr,  // glGenSamplers
    nullptr,  // glDeleteSamplers
    nullptr,  // glIsSampler
    nullptr,  // glBindSampler
    nullptr,  // glSamplerParameteri
    nullptr,  // glSamplerParameteriv
    nullptr,  // glSamplerParameterf
    nullptr,  // glSamplerParameterfv
    nullptr,  // glGetSamplerParameteriv
    nullptr,  // glGetSamplerParameterfv
    nullptr,  // glVertexAttribDivisor
    nullptr,  // glBindTransformFeedback
    nullptr,  // glDeleteTransformFeedbacks
    nullptr,  // glGenTransformFeedbacks
    nullptr,  // glIsTransformFeedback
    nullptr,  // glPauseTransformFeedback
    nullptr,  // glResumeTransformFeedback
    nullptr,  // glGetProgramBinary
    nullptr,  // glProgramBinary
    nullptr,  // glProgramParameteri
    nullptr,  // glInvalidateFramebuffer
    nullptr,  // glInvalidateSubFramebuffer
    nullptr,  // glTexStorage2D
    nullptr,  // glTexStorage3D
    nullptr,  // glGetInternalformativ
};

}  // namespace

const SbGlesInterface* SbGetGlesInterface() {
  return &g_sb_gles_interface;
}
