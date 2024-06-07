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

#include "starboard/common/log.h"
#include "starboard/gles.h"

extern "C" {
#define GR_GL_TEXTURE_MAX_LEVEL 0x813D
#define STR_(a) \
  case a:       \
    return #a
const char* _target2str(GLenum target) {
  switch (target) { STR_(GL_TEXTURE_2D); }
  return "UNLISTED";
}
const char* _param2str(GLenum pname) {
  switch (pname) {
    STR_(GL_TEXTURE_WRAP_S);
    STR_(GL_TEXTURE_WRAP_T);
    STR_(GL_TEXTURE_MIN_FILTER);
    STR_(GL_TEXTURE_MAG_FILTER);
    // STR_(GL_TEXTURE_USAGE);
    // STR_(GL_TEXTURE_BASE_LEVEL);
    STR_(GR_GL_TEXTURE_MAX_LEVEL);
  }
  return "UNLISTED";
}
const char* _value2str(GLint param) {
  switch (param) {
    STR_(GL_LINEAR);
    STR_(GL_CLAMP_TO_EDGE);
    STR_(GL_NEAREST);
    STR_(GL_REPEAT);
  }
  return "UNLISTED";
}
#undef STR_

void log_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
  SB_LOG(ERROR) << "glTexParameterf [" << target << "] (" << _target2str(target)
                << ") [" << pname << "] (" << _param2str(pname)
                << ") value:" << param << " [" << _value2str(param) << "]";
  glTexParameterf(target, pname, param);
}
void log_glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  SB_LOG(ERROR) << "glTexParameterfv [" << target << "] ("
                << _target2str(target) << ") [" << pname << "] ("
                << _param2str(pname) << ")";
  glTexParameterfv(target, pname, params);
}
void log_glTexParameteri(GLenum target, GLenum pname, GLint param) {
  SB_LOG(ERROR) << "glTexParameteri [" << target << "] (" << _target2str(target)
                << ") [" << pname << "] (" << _param2str(pname)
                << ") value:" << param << " [" << _value2str(param) << "]";
  glTexParameteri(target, pname, param);
}
void log_glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  SB_LOG(ERROR) << "glTexParameteriv [" << target << "] ("
                << _target2str(target) << ") [" << pname << "] ("
                << _param2str(pname) << ")";
  glTexParameteriv(target, pname, params);
}
const GLubyte* log_glGetString(GLenum name) {
  auto ret = glGetString(name);
  SB_LOG(ERROR) << "log_glGetString [" << name << "]";
  return ret;
}
}

namespace {

const SbGlesInterface g_sb_gles_interface = {
    &glActiveTexture,
    &glAttachShader,
    &glBindAttribLocation,
    &glBindBuffer,
    &glBindFramebuffer,
    &glBindRenderbuffer,
    &glBindTexture,
    &glBlendColor,
    &glBlendEquation,
    &glBlendEquationSeparate,
    &glBlendFunc,
    &glBlendFuncSeparate,
    &glBufferData,
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
    &glDeleteBuffers,
    &glDeleteFramebuffers,
    &glDeleteProgram,
    &glDeleteRenderbuffers,
    &glDeleteShader,
    &glDeleteTextures,
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
    &glGenBuffers,
    &glGenerateMipmap,
    &glGenFramebuffers,
    &glGenRenderbuffers,
    &glGenTextures,
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
    &glRenderbufferStorage,
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
    &glTexImage2D,
    &log_glTexParameterf,
    &log_glTexParameterfv,
    &log_glTexParameteri,
    &log_glTexParameteriv,
    &glTexSubImage2D,
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
