---
layout: doc
title: "Starboard Module Reference: gles.h"
---

The GLES API provides an interface with accompanying type declarations and
defines that together provide a single consistent method of GLES usage across
platforms.

This API is designed to abstract the differences between GLES implementations
and versions on different systems, and to remove the requirement for any other
code to directly pull in and use these system libraries.
GLES Version

This API has the ability to support GLES 3.0, however platforms are not required
to support anything beyond GLES 2.0. The caller is responsible for ensuring that
the functions from GLES 3.0 they are calling from the interface are valid.

## Macros ##

### SB_GL_DEPTH_BUFFER_BIT ###

Previously defined in [https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h](https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h)

### SB_GL_READ_BUFFER ###

Previously defined in [https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h](https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h)
.

## Typedefs ##

### SbGlBoolean ###

The following type definitions were adapted from the types declared in [https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h](https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h)
.

#### Definition ####

```
typedef uint8_t SbGlBoolean
```

### SbGlIntPtr ###

Some compilers will transform the intptr_t to an int transparently behind the
scenes, which is not equivalent to a long int, or long long int, as far as the
compiler is concerned. We check the Starboard configuration and set the types to
those exact types used by OpenGL ES 2.0 ( [https://www.khronos.org/registry/OpenGL/api/GLES2/gl2ext.h](https://www.khronos.org/registry/OpenGL/api/GLES2/gl2ext.h)
).

#### Definition ####

```
typedef long int SbGlIntPtr
```

## Structs ##

### SbGlesInterface ###

#### Members ####

*   `void(* glActiveTexture`

    The following prototypes were adapted from the prototypes declared in [https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h](https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h)
    .
*   `void(* glAttachShader`
*   `void(* glBindAttribLocation`
*   `void(* glBindBuffer`
*   `void(* glBindFramebuffer`
*   `void(* glBindRenderbuffer`
*   `void(* glBindTexture`
*   `void(* glBlendColor`
*   `void(* glBlendEquation`
*   `void(* glBlendEquationSeparate`
*   `void(* glBlendFunc`
*   `void(* glBlendFuncSeparate`
*   `void(* glBufferData`
*   `void(* glBufferSubData`
*   `SbGlEnum(* glCheckFramebufferStatus`
*   `void(* glClear`
*   `void(* glClearColor`
*   `void(* glClearDepthf`
*   `void(* glClearStencil`
*   `void(* glColorMask`
*   `void(* glCompileShader`
*   `void(* glCompressedTexImage2D`
*   `void(* glCompressedTexSubImage2D`
*   `void(* glCopyTexImage2D`
*   `void(* glCopyTexSubImage2D`
*   `SbGlUInt32(* glCreateProgram`
*   `SbGlUInt32(* glCreateShader`
*   `void(* glCullFace`
*   `void(* glDeleteBuffers`
*   `void(* glDeleteFramebuffers`
*   `void(* glDeleteProgram`
*   `void(* glDeleteRenderbuffers`
*   `void(* glDeleteShader`
*   `void(* glDeleteTextures`
*   `void(* glDepthFunc`
*   `void(* glDepthMask`
*   `void(* glDepthRangef`
*   `void(* glDetachShader`
*   `void(* glDisable`
*   `void(* glDisableVertexAttribArray`
*   `void(* glDrawArrays`
*   `void(* glDrawElements`
*   `void(* glEnable`
*   `void(* glEnableVertexAttribArray`
*   `void(* glFinish`
*   `void(* glFlush`
*   `void(* glFramebufferRenderbuffer`
*   `void(* glFramebufferTexture2D`
*   `void(* glFrontFace`
*   `void(* glGenBuffers`
*   `void(* glGenerateMipmap`
*   `void(* glGenFramebuffers`
*   `void(* glGenRenderbuffers`
*   `void(* glGenTextures`
*   `void(* glGetActiveAttrib`
*   `void(* glGetActiveUniform`
*   `void(* glGetAttachedShaders`
*   `SbGlInt32(* glGetAttribLocation`
*   `void(* glGetBooleanv`
*   `void(* glGetBufferParameteriv`
*   `SbGlEnum(* glGetError`
*   `void(* glGetFloatv`
*   `void(* glGetFramebufferAttachmentParameteriv`
*   `void(* glGetIntegerv`
*   `void(* glGetProgramiv`
*   `void(* glGetProgramInfoLog`
*   `void(* glGetRenderbufferParameteriv`
*   `void(* glGetShaderiv`
*   `void(* glGetShaderInfoLog`
*   `void(* glGetShaderPrecisionFormat`
*   `void(* glGetShaderSource`
*   `const SbGlUInt8 *(* glGetString`
*   `void(* glGetTexParameterfv`
*   `void(* glGetTexParameteriv`
*   `void(* glGetUniformfv`
*   `void(* glGetUniformiv`
*   `SbGlInt32(* glGetUniformLocation`
*   `void(* glGetVertexAttribfv`
*   `void(* glGetVertexAttribiv`
*   `void(* glGetVertexAttribPointerv`
*   `void(* glHint`
*   `SbGlBoolean(* glIsBuffer`
*   `SbGlBoolean(* glIsEnabled`
*   `SbGlBoolean(* glIsFramebuffer`
*   `SbGlBoolean(* glIsProgram`
*   `SbGlBoolean(* glIsRenderbuffer`
*   `SbGlBoolean(* glIsShader`
*   `SbGlBoolean(* glIsTexture`
*   `void(* glLineWidth`
*   `void(* glLinkProgram`
*   `void(* glPixelStorei`
*   `void(* glPolygonOffset`
*   `void(* glReadPixels`
*   `void(* glReleaseShaderCompiler`
*   `void(* glRenderbufferStorage`
*   `void(* glSampleCoverage`
*   `void(* glScissor`
*   `void(* glShaderBinary`
*   `void(* glShaderSource`
*   `void(* glStencilFunc`
*   `void(* glStencilFuncSeparate`
*   `void(* glStencilMask`
*   `void(* glStencilMaskSeparate`
*   `void(* glStencilOp`
*   `void(* glStencilOpSeparate`
*   `void(* glTexImage2D`
*   `void(* glTexParameterf`
*   `void(* glTexParameterfv`
*   `void(* glTexParameteri`
*   `void(* glTexParameteriv`
*   `void(* glTexSubImage2D`
*   `void(* glUniform1f`
*   `void(* glUniform1fv`
*   `void(* glUniform1i`
*   `void(* glUniform1iv`
*   `void(* glUniform2f`
*   `void(* glUniform2fv`
*   `void(* glUniform2i`
*   `void(* glUniform2iv`
*   `void(* glUniform3f`
*   `void(* glUniform3fv`
*   `void(* glUniform3i`
*   `void(* glUniform3iv`
*   `void(* glUniform4f`
*   `void(* glUniform4fv`
*   `void(* glUniform4i`
*   `void(* glUniform4iv`
*   `void(* glUniformMatrix2fv`
*   `void(* glUniformMatrix3fv`
*   `void(* glUniformMatrix4fv`
*   `void(* glUseProgram`
*   `void(* glValidateProgram`
*   `void(* glVertexAttrib1f`
*   `void(* glVertexAttrib1fv`
*   `void(* glVertexAttrib2f`
*   `void(* glVertexAttrib2fv`
*   `void(* glVertexAttrib3f`
*   `void(* glVertexAttrib3fv`
*   `void(* glVertexAttrib4f`
*   `void(* glVertexAttrib4fv`
*   `void(* glVertexAttribPointer`
*   `void(* glViewport`
*   `void(* glReadBuffer`

    The following prototypes were adapted from the prototypes declared in [https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h](https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h)
    .
*   `void(* glDrawRangeElements`
*   `void(* glTexImage3D`
*   `void(* glTexSubImage3D`
*   `void(* glCopyTexSubImage3D`
*   `void(* glCompressedTexImage3D`
*   `void(* glCompressedTexSubImage3D`
*   `void(* glGenQueries`
*   `void(* glDeleteQueries`
*   `SbGlBoolean(* glIsQuery`
*   `void(* glBeginQuery`
*   `void(* glEndQuery`
*   `void(* glGetQueryiv`
*   `void(* glGetQueryObjectuiv`
*   `SbGlBoolean(* glUnmapBuffer`
*   `void(* glGetBufferPointerv`
*   `void(* glDrawBuffers`
*   `void(* glUniformMatrix2x3fv`
*   `void(* glUniformMatrix3x2fv`
*   `void(* glUniformMatrix2x4fv`
*   `void(* glUniformMatrix4x2fv`
*   `void(* glUniformMatrix3x4fv`
*   `void(* glUniformMatrix4x3fv`
*   `void(* glBlitFramebuffer`
*   `void(* glRenderbufferStorageMultisample`
*   `void(* glFramebufferTextureLayer`
*   `void *(* glMapBufferRange`
*   `void(* glFlushMappedBufferRange`
*   `void(* glBindVertexArray`
*   `void(* glDeleteVertexArrays`
*   `void(* glGenVertexArrays`
*   `SbGlBoolean(* glIsVertexArray`
*   `void(* glGetIntegeri_v`
*   `void(* glBeginTransformFeedback`
*   `void(* glEndTransformFeedback`
*   `void(* glBindBufferRange`
*   `void(* glBindBufferBase`
*   `void(* glTransformFeedbackVaryings`
*   `void(* glGetTransformFeedbackVarying`
*   `void(* glVertexAttribIPointer`
*   `void(* glGetVertexAttribIiv`
*   `void(* glGetVertexAttribIuiv`
*   `void(* glVertexAttribI4i`
*   `void(* glVertexAttribI4ui`
*   `void(* glVertexAttribI4iv`
*   `void(* glVertexAttribI4uiv`
*   `void(* glGetUniformuiv`
*   `SbGlInt32(* glGetFragDataLocation`
*   `void(* glUniform1ui`
*   `void(* glUniform2ui`
*   `void(* glUniform3ui`
*   `void(* glUniform4ui`
*   `void(* glUniform1uiv`
*   `void(* glUniform2uiv`
*   `void(* glUniform3uiv`
*   `void(* glUniform4uiv`
*   `void(* glClearBufferiv`
*   `void(* glClearBufferuiv`
*   `void(* glClearBufferfv`
*   `void(* glClearBufferfi`
*   `const SbGlUInt8 *(* glGetStringi`
*   `void(* glCopyBufferSubData`
*   `void(* glGetUniformIndices`
*   `void(* glGetActiveUniformsiv`
*   `SbGlUInt32(* glGetUniformBlockIndex`
*   `void(* glGetActiveUniformBlockiv`
*   `void(* glGetActiveUniformBlockName`
*   `void(* glUniformBlockBinding`
*   `void(* glDrawArraysInstanced`
*   `void(* glDrawElementsInstanced`
*   `SbGlSync(* glFenceSync`
*   `SbGlBoolean(* glIsSync`
*   `void(* glDeleteSync`
*   `SbGlEnum(* glClientWaitSync`
*   `void(* glWaitSync`
*   `void(* glGetInteger64v`
*   `void(* glGetSynciv`
*   `void(* glGetInteger64i_v`
*   `void(* glGetBufferParameteri64v`
*   `void(* glGenSamplers`
*   `void(* glDeleteSamplers`
*   `SbGlBoolean(* glIsSampler`
*   `void(* glBindSampler`
*   `void(* glSamplerParameteri`
*   `void(* glSamplerParameteriv`
*   `void(* glSamplerParameterf`
*   `void(* glSamplerParameterfv`
*   `void(* glGetSamplerParameteriv`
*   `void(* glGetSamplerParameterfv`
*   `void(* glVertexAttribDivisor`
*   `void(* glBindTransformFeedback`
*   `void(* glDeleteTransformFeedbacks`
*   `void(* glGenTransformFeedbacks`
*   `SbGlBoolean(* glIsTransformFeedback`
*   `void(* glPauseTransformFeedback`
*   `void(* glResumeTransformFeedback`
*   `void(* glGetProgramBinary`
*   `void(* glProgramBinary`
*   `void(* glProgramParameteri`
*   `void(* glInvalidateFramebuffer`
*   `void(* glInvalidateSubFramebuffer`
*   `void(* glTexStorage2D`
*   `void(* glTexStorage3D`
*   `void(* glGetInternalformativ`

