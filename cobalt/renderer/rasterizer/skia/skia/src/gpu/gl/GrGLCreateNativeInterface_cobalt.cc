// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "third_party/skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "third_party/skia/src/gpu/gl/GrGLUtil.h"

#define REGISTER_GL_FUNCTION(F)                   \
                                                  \
  if (0 == strcmp("gl" #F, name)) {               \
    return reinterpret_cast<GrGLFuncPtr>(&gl##F); \
  }

GrGLFuncPtr GetGLProc(void* ctx, const char name[]) {
  SkASSERT(NULL == ctx);

  // Make non-extension GL function addresses available without having
  // to go through eglGetProcAddress() if possible.
  REGISTER_GL_FUNCTION(ActiveTexture);
  REGISTER_GL_FUNCTION(AttachShader);
  REGISTER_GL_FUNCTION(BindAttribLocation);
  REGISTER_GL_FUNCTION(BindBuffer);
  REGISTER_GL_FUNCTION(BindTexture);
  REGISTER_GL_FUNCTION(BlendColor);
  REGISTER_GL_FUNCTION(BlendEquation);
  REGISTER_GL_FUNCTION(BlendFunc);
  REGISTER_GL_FUNCTION(BufferData);
  REGISTER_GL_FUNCTION(BufferSubData);
  REGISTER_GL_FUNCTION(Clear);
  REGISTER_GL_FUNCTION(ClearColor);
  REGISTER_GL_FUNCTION(ClearStencil);
  REGISTER_GL_FUNCTION(ColorMask);
  REGISTER_GL_FUNCTION(CompileShader);
  REGISTER_GL_FUNCTION(CompressedTexImage2D);
  REGISTER_GL_FUNCTION(CompressedTexSubImage2D);
  REGISTER_GL_FUNCTION(CopyTexSubImage2D);
  REGISTER_GL_FUNCTION(CreateProgram);
  REGISTER_GL_FUNCTION(CreateShader);
  REGISTER_GL_FUNCTION(CullFace);
  REGISTER_GL_FUNCTION(DeleteBuffers);
  REGISTER_GL_FUNCTION(DeleteProgram);
  REGISTER_GL_FUNCTION(DeleteShader);
  REGISTER_GL_FUNCTION(DeleteTextures);
  REGISTER_GL_FUNCTION(DepthMask);
  REGISTER_GL_FUNCTION(Disable);
  REGISTER_GL_FUNCTION(DisableVertexAttribArray);
  REGISTER_GL_FUNCTION(DrawArrays);
  REGISTER_GL_FUNCTION(DrawElements);
  REGISTER_GL_FUNCTION(Enable);
  REGISTER_GL_FUNCTION(EnableVertexAttribArray);
  REGISTER_GL_FUNCTION(FrontFace);
  REGISTER_GL_FUNCTION(GenBuffers);
  REGISTER_GL_FUNCTION(GenTextures);
  REGISTER_GL_FUNCTION(GetBufferParameteriv);
  REGISTER_GL_FUNCTION(GenerateMipmap);
  REGISTER_GL_FUNCTION(GetError);
  REGISTER_GL_FUNCTION(GetIntegerv);
  REGISTER_GL_FUNCTION(GetProgramInfoLog);
  REGISTER_GL_FUNCTION(GetProgramiv);
  REGISTER_GL_FUNCTION(GetShaderInfoLog);
  REGISTER_GL_FUNCTION(GetShaderiv);
  REGISTER_GL_FUNCTION(GetString);
  REGISTER_GL_FUNCTION(GetUniformLocation);
  REGISTER_GL_FUNCTION(IsTexture);
  REGISTER_GL_FUNCTION(LinkProgram);
  REGISTER_GL_FUNCTION(LineWidth);
  REGISTER_GL_FUNCTION(PixelStorei);
  REGISTER_GL_FUNCTION(ReadPixels);
  REGISTER_GL_FUNCTION(Scissor);
  REGISTER_GL_FUNCTION(ShaderSource);
  REGISTER_GL_FUNCTION(StencilFunc);
  REGISTER_GL_FUNCTION(StencilFuncSeparate);
  REGISTER_GL_FUNCTION(StencilMask);
  REGISTER_GL_FUNCTION(StencilMaskSeparate);
  REGISTER_GL_FUNCTION(StencilOp);
  REGISTER_GL_FUNCTION(StencilOpSeparate);
  REGISTER_GL_FUNCTION(TexImage2D);
  REGISTER_GL_FUNCTION(TexParameteri);
  REGISTER_GL_FUNCTION(TexParameteriv);
  REGISTER_GL_FUNCTION(TexSubImage2D);
  REGISTER_GL_FUNCTION(Uniform1f);
  REGISTER_GL_FUNCTION(Uniform1i);
  REGISTER_GL_FUNCTION(Uniform1fv);
  REGISTER_GL_FUNCTION(Uniform1iv);
  REGISTER_GL_FUNCTION(Uniform2f);
  REGISTER_GL_FUNCTION(Uniform2i);
  REGISTER_GL_FUNCTION(Uniform2fv);
  REGISTER_GL_FUNCTION(Uniform2iv);
  REGISTER_GL_FUNCTION(Uniform3f);
  REGISTER_GL_FUNCTION(Uniform3i);
  REGISTER_GL_FUNCTION(Uniform3fv);
  REGISTER_GL_FUNCTION(Uniform3iv);
  REGISTER_GL_FUNCTION(Uniform4f);
  REGISTER_GL_FUNCTION(Uniform4i);
  REGISTER_GL_FUNCTION(Uniform4fv);
  REGISTER_GL_FUNCTION(Uniform4iv);
  REGISTER_GL_FUNCTION(UniformMatrix2fv);
  REGISTER_GL_FUNCTION(UniformMatrix3fv);
  REGISTER_GL_FUNCTION(UniformMatrix4fv);
  REGISTER_GL_FUNCTION(UseProgram);
  REGISTER_GL_FUNCTION(VertexAttrib1f);
  REGISTER_GL_FUNCTION(VertexAttrib2fv);
  REGISTER_GL_FUNCTION(VertexAttrib3fv);
  REGISTER_GL_FUNCTION(VertexAttrib4fv);
  REGISTER_GL_FUNCTION(VertexAttribPointer);
  REGISTER_GL_FUNCTION(Viewport);
  REGISTER_GL_FUNCTION(BindFramebuffer);
  REGISTER_GL_FUNCTION(BindRenderbuffer);
  REGISTER_GL_FUNCTION(CheckFramebufferStatus);
  REGISTER_GL_FUNCTION(DeleteFramebuffers);
  REGISTER_GL_FUNCTION(DeleteRenderbuffers);
  REGISTER_GL_FUNCTION(Finish);
  REGISTER_GL_FUNCTION(Flush);
  REGISTER_GL_FUNCTION(FramebufferRenderbuffer);
  REGISTER_GL_FUNCTION(FramebufferTexture2D);
  REGISTER_GL_FUNCTION(GetFramebufferAttachmentParameteriv);
  REGISTER_GL_FUNCTION(GetRenderbufferParameteriv);
  REGISTER_GL_FUNCTION(GenFramebuffers);
  REGISTER_GL_FUNCTION(GenRenderbuffers);
  REGISTER_GL_FUNCTION(RenderbufferStorage);
  REGISTER_GL_FUNCTION(GetShaderPrecisionFormat);

  return eglGetProcAddress(name);
}

const GrGLInterface* GrGLCreateNativeInterface() {
  return GrGLAssembleInterface(NULL, &GetGLProc);
}
