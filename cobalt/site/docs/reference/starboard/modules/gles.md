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

## GLES Version ##

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

*   `void(*glActiveTexture)(SbGlEnum texture)`

    The following prototypes were adapted from the prototypes declared in [https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h](https://www.khronos.org/registry/OpenGL/api/GLES2/gl2.h)
    .
*   `void(*glAttachShader)(SbGlUInt32 program, SbGlUInt32 shader)`
*   `void(*glBindAttribLocation)(SbGlUInt32 program, SbGlUInt32 index, const
    SbGlChar *name)`
*   `void(*glBindBuffer)(SbGlEnum target, SbGlUInt32 buffer)`
*   `void(*glBindFramebuffer)(SbGlEnum target, SbGlUInt32 framebuffer)`
*   `void(*glBindRenderbuffer)(SbGlEnum target, SbGlUInt32 renderbuffer)`
*   `void(*glBindTexture)(SbGlEnum target, SbGlUInt32 texture)`
*   `void(*glBlendColor)(SbGlFloat red, SbGlFloat green, SbGlFloat blue,
    SbGlFloat alpha)`
*   `void(*glBlendEquation)(SbGlEnum mode)`
*   `void(*glBlendEquationSeparate)(SbGlEnum modeRGB, SbGlEnum modeAlpha)`
*   `void(*glBlendFunc)(SbGlEnum sfactor, SbGlEnum dfactor)`
*   `void(*glBlendFuncSeparate)(SbGlEnum sfactorRGB, SbGlEnum dfactorRGB,
    SbGlEnum sfactorAlpha, SbGlEnum dfactorAlpha)`
*   `void(*glBufferData)(SbGlEnum target, SbGlSizeiPtr size, const void *data,
    SbGlEnum usage)`
*   `void(*glBufferSubData)(SbGlEnum target, SbGlIntPtr offset, SbGlSizeiPtr
    size, const void *data)`
*   `SbGlEnum(*glCheckFramebufferStatus)(SbGlEnum target)`
*   `void(*glClear)(SbGlBitfield mask)`
*   `void(*glClearColor)(SbGlFloat red, SbGlFloat green, SbGlFloat blue,
    SbGlFloat alpha)`
*   `void(*glClearDepthf)(SbGlFloat d)`
*   `void(*glClearStencil)(SbGlInt32 s)`
*   `void(*glColorMask)(SbGlBoolean red, SbGlBoolean green, SbGlBoolean blue,
    SbGlBoolean alpha)`
*   `void(*glCompileShader)(SbGlUInt32 shader)`
*   `void(*glCompressedTexImage2D)(SbGlEnum target, SbGlInt32 level, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height, SbGlInt32 border,
    SbGlSizei imageSize, const void *data)`
*   `void(*glCompressedTexSubImage2D)(SbGlEnum target, SbGlInt32 level,
    SbGlInt32 xoffset, SbGlInt32 yoffset, SbGlSizei width, SbGlSizei height,
    SbGlEnum format, SbGlSizei imageSize, const void *data)`
*   `void(*glCopyTexImage2D)(SbGlEnum target, SbGlInt32 level, SbGlEnum
    internalformat, SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei height,
    SbGlInt32 border)`
*   `void(*glCopyTexSubImage2D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    xoffset, SbGlInt32 yoffset, SbGlInt32 x, SbGlInt32 y, SbGlSizei width,
    SbGlSizei height)`
*   `SbGlUInt32(*glCreateProgram)(void)`
*   `SbGlUInt32(*glCreateShader)(SbGlEnum type)`
*   `void(*glCullFace)(SbGlEnum mode)`
*   `void(*glDeleteBuffers)(SbGlSizei n, const SbGlUInt32 *buffers)`
*   `void(*glDeleteFramebuffers)(SbGlSizei n, const SbGlUInt32 *framebuffers)`
*   `void(*glDeleteProgram)(SbGlUInt32 program)`
*   `void(*glDeleteRenderbuffers)(SbGlSizei n, const SbGlUInt32 *renderbuffers)`
*   `void(*glDeleteShader)(SbGlUInt32 shader)`
*   `void(*glDeleteTextures)(SbGlSizei n, const SbGlUInt32 *textures)`
*   `void(*glDepthFunc)(SbGlEnum func)`
*   `void(*glDepthMask)(SbGlBoolean flag)`
*   `void(*glDepthRangef)(SbGlFloat n, SbGlFloat f)`
*   `void(*glDetachShader)(SbGlUInt32 program, SbGlUInt32 shader)`
*   `void(*glDisable)(SbGlEnum cap)`
*   `void(*glDisableVertexAttribArray)(SbGlUInt32 index)`
*   `void(*glDrawArrays)(SbGlEnum mode, SbGlInt32 first, SbGlSizei count)`
*   `void(*glDrawElements)(SbGlEnum mode, SbGlSizei count, SbGlEnum type, const
    void *indices)`
*   `void(*glEnable)(SbGlEnum cap)`
*   `void(*glEnableVertexAttribArray)(SbGlUInt32 index)`
*   `void(*glFinish)(void)`
*   `void(*glFlush)(void)`
*   `void(*glFramebufferRenderbuffer)(SbGlEnum target, SbGlEnum attachment,
    SbGlEnum renderbuffertarget, SbGlUInt32 renderbuffer)`
*   `void(*glFramebufferTexture2D)(SbGlEnum target, SbGlEnum attachment,
    SbGlEnum textarget, SbGlUInt32 texture, SbGlInt32 level)`
*   `void(*glFrontFace)(SbGlEnum mode)`
*   `void(*glGenBuffers)(SbGlSizei n, SbGlUInt32 *buffers)`
*   `void(*glGenerateMipmap)(SbGlEnum target)`
*   `void(*glGenFramebuffers)(SbGlSizei n, SbGlUInt32 *framebuffers)`
*   `void(*glGenRenderbuffers)(SbGlSizei n, SbGlUInt32 *renderbuffers)`
*   `void(*glGenTextures)(SbGlSizei n, SbGlUInt32 *textures)`
*   `void(*glGetActiveAttrib)(SbGlUInt32 program, SbGlUInt32 index, SbGlSizei
    bufSize, SbGlSizei *length, SbGlInt32 *size, SbGlEnum *type, SbGlChar
    *name)`
*   `void(*glGetActiveUniform)(SbGlUInt32 program, SbGlUInt32 index, SbGlSizei
    bufSize, SbGlSizei *length, SbGlInt32 *size, SbGlEnum *type, SbGlChar
    *name)`
*   `void(*glGetAttachedShaders)(SbGlUInt32 program, SbGlSizei maxCount,
    SbGlSizei *count, SbGlUInt32 *shaders)`
*   `SbGlInt32(*glGetAttribLocation)(SbGlUInt32 program, const SbGlChar *name)`
*   `void(*glGetBooleanv)(SbGlEnum pname, SbGlBoolean *data)`
*   `void(*glGetBufferParameteriv)(SbGlEnum target, SbGlEnum pname, SbGlInt32
    *params)`
*   `SbGlEnum(*glGetError)(void)`
*   `void(*glGetFloatv)(SbGlEnum pname, SbGlFloat *data)`
*   `void(*glGetFramebufferAttachmentParameteriv)(SbGlEnum target, SbGlEnum
    attachment, SbGlEnum pname, SbGlInt32 *params)`
*   `void(*glGetIntegerv)(SbGlEnum pname, SbGlInt32 *data)`
*   `void(*glGetProgramiv)(SbGlUInt32 program, SbGlEnum pname, SbGlInt32
    *params)`
*   `void(*glGetProgramInfoLog)(SbGlUInt32 program, SbGlSizei bufSize, SbGlSizei
    *length, SbGlChar *infoLog)`
*   `void(*glGetRenderbufferParameteriv)(SbGlEnum target, SbGlEnum pname,
    SbGlInt32 *params)`
*   `void(*glGetShaderiv)(SbGlUInt32 shader, SbGlEnum pname, SbGlInt32 *params)`
*   `void(*glGetShaderInfoLog)(SbGlUInt32 shader, SbGlSizei bufSize, SbGlSizei
    *length, SbGlChar *infoLog)`
*   `void(*glGetShaderPrecisionFormat)(SbGlEnum shadertype, SbGlEnum
    precisiontype, SbGlInt32 *range, SbGlInt32 *precision)`
*   `void(*glGetShaderSource)(SbGlUInt32 shader, SbGlSizei bufSize, SbGlSizei
    *length, SbGlChar *source)`
*   `const SbGlUInt8 *(*glGetString)(SbGlEnum name)`
*   `void(*glGetTexParameterfv)(SbGlEnum target, SbGlEnum pname, SbGlFloat
    *params)`
*   `void(*glGetTexParameteriv)(SbGlEnum target, SbGlEnum pname, SbGlInt32
    *params)`
*   `void(*glGetUniformfv)(SbGlUInt32 program, SbGlInt32 location, SbGlFloat
    *params)`
*   `void(*glGetUniformiv)(SbGlUInt32 program, SbGlInt32 location, SbGlInt32
    *params)`
*   `SbGlInt32(*glGetUniformLocation)(SbGlUInt32 program, const SbGlChar *name)`
*   `void(*glGetVertexAttribfv)(SbGlUInt32 index, SbGlEnum pname, SbGlFloat
    *params)`
*   `void(*glGetVertexAttribiv)(SbGlUInt32 index, SbGlEnum pname, SbGlInt32
    *params)`
*   `void(*glGetVertexAttribPointerv)(SbGlUInt32 index, SbGlEnum pname, void
    **pointer)`
*   `void(*glHint)(SbGlEnum target, SbGlEnum mode)`
*   `SbGlBoolean(*glIsBuffer)(SbGlUInt32 buffer)`
*   `SbGlBoolean(*glIsEnabled)(SbGlEnum cap)`
*   `SbGlBoolean(*glIsFramebuffer)(SbGlUInt32 framebuffer)`
*   `SbGlBoolean(*glIsProgram)(SbGlUInt32 program)`
*   `SbGlBoolean(*glIsRenderbuffer)(SbGlUInt32 renderbuffer)`
*   `SbGlBoolean(*glIsShader)(SbGlUInt32 shader)`
*   `SbGlBoolean(*glIsTexture)(SbGlUInt32 texture)`
*   `void(*glLineWidth)(SbGlFloat width)`
*   `void(*glLinkProgram)(SbGlUInt32 program)`
*   `void(*glPixelStorei)(SbGlEnum pname, SbGlInt32 param)`
*   `void(*glPolygonOffset)(SbGlFloat factor, SbGlFloat units)`
*   `void(*glReadPixels)(SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei
    height, SbGlEnum format, SbGlEnum type, void *pixels)`
*   `void(*glReleaseShaderCompiler)(void)`
*   `void(*glRenderbufferStorage)(SbGlEnum target, SbGlEnum internalformat,
    SbGlSizei width, SbGlSizei height)`
*   `void(*glSampleCoverage)(SbGlFloat value, SbGlBoolean invert)`
*   `void(*glScissor)(SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei
    height)`
*   `void(*glShaderBinary)(SbGlSizei count, const SbGlUInt32 *shaders, SbGlEnum
    binaryformat, const void *binary, SbGlSizei length)`
*   `void(*glShaderSource)(SbGlUInt32 shader, SbGlSizei count, const SbGlChar
    *const *string, const SbGlInt32 *length)`
*   `void(*glStencilFunc)(SbGlEnum func, SbGlInt32 ref, SbGlUInt32 mask)`
*   `void(*glStencilFuncSeparate)(SbGlEnum face, SbGlEnum func, SbGlInt32 ref,
    SbGlUInt32 mask)`
*   `void(*glStencilMask)(SbGlUInt32 mask)`
*   `void(*glStencilMaskSeparate)(SbGlEnum face, SbGlUInt32 mask)`
*   `void(*glStencilOp)(SbGlEnum fail, SbGlEnum zfail, SbGlEnum zpass)`
*   `void(*glStencilOpSeparate)(SbGlEnum face, SbGlEnum sfail, SbGlEnum dpfail,
    SbGlEnum dppass)`
*   `void(*glTexImage2D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    internalformat, SbGlSizei width, SbGlSizei height, SbGlInt32 border,
    SbGlEnum format, SbGlEnum type, const void *pixels)`
*   `void(*glTexParameterf)(SbGlEnum target, SbGlEnum pname, SbGlFloat param)`
*   `void(*glTexParameterfv)(SbGlEnum target, SbGlEnum pname, const SbGlFloat
    *params)`
*   `void(*glTexParameteri)(SbGlEnum target, SbGlEnum pname, SbGlInt32 param)`
*   `void(*glTexParameteriv)(SbGlEnum target, SbGlEnum pname, const SbGlInt32
    *params)`
*   `void(*glTexSubImage2D)(SbGlEnum target, SbGlInt32 level, SbGlInt32 xoffset,
    SbGlInt32 yoffset, SbGlSizei width, SbGlSizei height, SbGlEnum format,
    SbGlEnum type, const void *pixels)`
*   `void(*glUniform1f)(SbGlInt32 location, SbGlFloat v0)`
*   `void(*glUniform1fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat
    *value)`
*   `void(*glUniform1i)(SbGlInt32 location, SbGlInt32 v0)`
*   `void(*glUniform1iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32
    *value)`
*   `void(*glUniform2f)(SbGlInt32 location, SbGlFloat v0, SbGlFloat v1)`
*   `void(*glUniform2fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat
    *value)`
*   `void(*glUniform2i)(SbGlInt32 location, SbGlInt32 v0, SbGlInt32 v1)`
*   `void(*glUniform2iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32
    *value)`
*   `void(*glUniform3f)(SbGlInt32 location, SbGlFloat v0, SbGlFloat v1,
    SbGlFloat v2)`
*   `void(*glUniform3fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat
    *value)`
*   `void(*glUniform3i)(SbGlInt32 location, SbGlInt32 v0, SbGlInt32 v1,
    SbGlInt32 v2)`
*   `void(*glUniform3iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32
    *value)`
*   `void(*glUniform4f)(SbGlInt32 location, SbGlFloat v0, SbGlFloat v1,
    SbGlFloat v2, SbGlFloat v3)`
*   `void(*glUniform4fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat
    *value)`
*   `void(*glUniform4i)(SbGlInt32 location, SbGlInt32 v0, SbGlInt32 v1,
    SbGlInt32 v2, SbGlInt32 v3)`
*   `void(*glUniform4iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32
    *value)`
*   `void(*glUniformMatrix2fv)(SbGlInt32 location, SbGlSizei count, SbGlBoolean
    transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix3fv)(SbGlInt32 location, SbGlSizei count, SbGlBoolean
    transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix4fv)(SbGlInt32 location, SbGlSizei count, SbGlBoolean
    transpose, const SbGlFloat *value)`
*   `void(*glUseProgram)(SbGlUInt32 program)`
*   `void(*glValidateProgram)(SbGlUInt32 program)`
*   `void(*glVertexAttrib1f)(SbGlUInt32 index, SbGlFloat x)`
*   `void(*glVertexAttrib1fv)(SbGlUInt32 index, const SbGlFloat *v)`
*   `void(*glVertexAttrib2f)(SbGlUInt32 index, SbGlFloat x, SbGlFloat y)`
*   `void(*glVertexAttrib2fv)(SbGlUInt32 index, const SbGlFloat *v)`
*   `void(*glVertexAttrib3f)(SbGlUInt32 index, SbGlFloat x, SbGlFloat y,
    SbGlFloat z)`
*   `void(*glVertexAttrib3fv)(SbGlUInt32 index, const SbGlFloat *v)`
*   `void(*glVertexAttrib4f)(SbGlUInt32 index, SbGlFloat x, SbGlFloat y,
    SbGlFloat z, SbGlFloat w)`
*   `void(*glVertexAttrib4fv)(SbGlUInt32 index, const SbGlFloat *v)`
*   `void(*glVertexAttribPointer)(SbGlUInt32 index, SbGlInt32 size, SbGlEnum
    type, SbGlBoolean normalized, SbGlSizei stride, const void *pointer)`
*   `void(*glViewport)(SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei
    height)`
*   `void(*glReadBuffer)(SbGlEnum src)`

    The following prototypes were adapted from the prototypes declared in [https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h](https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h)
    .
*   `void(*glDrawRangeElements)(SbGlEnum mode, SbGlUInt32 start, SbGlUInt32 end,
    SbGlSizei count, SbGlEnum type, const void *indices)`
*   `void(*glTexImage3D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    internalformat, SbGlSizei width, SbGlSizei height, SbGlSizei depth,
    SbGlInt32 border, SbGlEnum format, SbGlEnum type, const void *pixels)`
*   `void(*glTexSubImage3D)(SbGlEnum target, SbGlInt32 level, SbGlInt32 xoffset,
    SbGlInt32 yoffset, SbGlInt32 zoffset, SbGlSizei width, SbGlSizei height,
    SbGlSizei depth, SbGlEnum format, SbGlEnum type, const void *pixels)`
*   `void(*glCopyTexSubImage3D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    xoffset, SbGlInt32 yoffset, SbGlInt32 zoffset, SbGlInt32 x, SbGlInt32 y,
    SbGlSizei width, SbGlSizei height)`
*   `void(*glCompressedTexImage3D)(SbGlEnum target, SbGlInt32 level, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height, SbGlSizei depth,
    SbGlInt32 border, SbGlSizei imageSize, const void *data)`
*   `void(*glCompressedTexSubImage3D)(SbGlEnum target, SbGlInt32 level,
    SbGlInt32 xoffset, SbGlInt32 yoffset, SbGlInt32 zoffset, SbGlSizei width,
    SbGlSizei height, SbGlSizei depth, SbGlEnum format, SbGlSizei imageSize,
    const void *data)`
*   `void(*glGenQueries)(SbGlSizei n, SbGlUInt32 *ids)`
*   `void(*glDeleteQueries)(SbGlSizei n, const SbGlUInt32 *ids)`
*   `SbGlBoolean(*glIsQuery)(SbGlUInt32 id)`
*   `void(*glBeginQuery)(SbGlEnum target, SbGlUInt32 id)`
*   `void(*glEndQuery)(SbGlEnum target)`
*   `void(*glGetQueryiv)(SbGlEnum target, SbGlEnum pname, SbGlInt32 *params)`
*   `void(*glGetQueryObjectuiv)(SbGlUInt32 id, SbGlEnum pname, SbGlUInt32
    *params)`
*   `SbGlBoolean(*glUnmapBuffer)(SbGlEnum target)`
*   `void(*glGetBufferPointerv)(SbGlEnum target, SbGlEnum pname, void **params)`
*   `void(*glDrawBuffers)(SbGlSizei n, const SbGlEnum *bufs)`
*   `void(*glUniformMatrix2x3fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix3x2fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix2x4fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix4x2fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix3x4fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat *value)`
*   `void(*glUniformMatrix4x3fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat *value)`
*   `void(*glBlitFramebuffer)(SbGlInt32 srcX0, SbGlInt32 srcY0, SbGlInt32 srcX1,
    SbGlInt32 srcY1, SbGlInt32 dstX0, SbGlInt32 dstY0, SbGlInt32 dstX1,
    SbGlInt32 dstY1, SbGlBitfield mask, SbGlEnum filter)`
*   `void(*glRenderbufferStorageMultisample)(SbGlEnum target, SbGlSizei samples,
    SbGlEnum internalformat, SbGlSizei width, SbGlSizei height)`
*   `void(*glFramebufferTextureLayer)(SbGlEnum target, SbGlEnum attachment,
    SbGlUInt32 texture, SbGlInt32 level, SbGlInt32 layer)`
*   `void *(*glMapBufferRange)(SbGlEnum target, SbGlIntPtr offset, SbGlSizeiPtr
    length, SbGlBitfield access)`
*   `void(*glFlushMappedBufferRange)(SbGlEnum target, SbGlIntPtr offset,
    SbGlSizeiPtr length)`
*   `void(*glBindVertexArray)(SbGlUInt32 array)`
*   `void(*glDeleteVertexArrays)(SbGlSizei n, const SbGlUInt32 *arrays)`
*   `void(*glGenVertexArrays)(SbGlSizei n, SbGlUInt32 *arrays)`
*   `SbGlBoolean(*glIsVertexArray)(SbGlUInt32 array)`
*   `void(*glGetIntegeri_v)(SbGlEnum target, SbGlUInt32 index, SbGlInt32 *data)`
*   `void(*glBeginTransformFeedback)(SbGlEnum primitiveMode)`
*   `void(*glEndTransformFeedback)(void)`
*   `void(*glBindBufferRange)(SbGlEnum target, SbGlUInt32 index, SbGlUInt32
    buffer, SbGlIntPtr offset, SbGlSizeiPtr size)`
*   `void(*glBindBufferBase)(SbGlEnum target, SbGlUInt32 index, SbGlUInt32
    buffer)`
*   `void(*glTransformFeedbackVaryings)(SbGlUInt32 program, SbGlSizei count,
    const SbGlChar *const *varyings, SbGlEnum bufferMode)`
*   `void(*glGetTransformFeedbackVarying)(SbGlUInt32 program, SbGlUInt32 index,
    SbGlSizei bufSize, SbGlSizei *length, SbGlSizei *size, SbGlEnum *type,
    SbGlChar *name)`
*   `void(*glVertexAttribIPointer)(SbGlUInt32 index, SbGlInt32 size, SbGlEnum
    type, SbGlSizei stride, const void *pointer)`
*   `void(*glGetVertexAttribIiv)(SbGlUInt32 index, SbGlEnum pname, SbGlInt32
    *params)`
*   `void(*glGetVertexAttribIuiv)(SbGlUInt32 index, SbGlEnum pname, SbGlUInt32
    *params)`
*   `void(*glVertexAttribI4i)(SbGlUInt32 index, SbGlInt32 x, SbGlInt32 y,
    SbGlInt32 z, SbGlInt32 w)`
*   `void(*glVertexAttribI4ui)(SbGlUInt32 index, SbGlUInt32 x, SbGlUInt32 y,
    SbGlUInt32 z, SbGlUInt32 w)`
*   `void(*glVertexAttribI4iv)(SbGlUInt32 index, const SbGlInt32 *v)`
*   `void(*glVertexAttribI4uiv)(SbGlUInt32 index, const SbGlUInt32 *v)`
*   `void(*glGetUniformuiv)(SbGlUInt32 program, SbGlInt32 location, SbGlUInt32
    *params)`
*   `SbGlInt32(*glGetFragDataLocation)(SbGlUInt32 program, const SbGlChar
    *name)`
*   `void(*glUniform1ui)(SbGlInt32 location, SbGlUInt32 v0)`
*   `void(*glUniform2ui)(SbGlInt32 location, SbGlUInt32 v0, SbGlUInt32 v1)`
*   `void(*glUniform3ui)(SbGlInt32 location, SbGlUInt32 v0, SbGlUInt32 v1,
    SbGlUInt32 v2)`
*   `void(*glUniform4ui)(SbGlInt32 location, SbGlUInt32 v0, SbGlUInt32 v1,
    SbGlUInt32 v2, SbGlUInt32 v3)`
*   `void(*glUniform1uiv)(SbGlInt32 location, SbGlSizei count, const SbGlUInt32
    *value)`
*   `void(*glUniform2uiv)(SbGlInt32 location, SbGlSizei count, const SbGlUInt32
    *value)`
*   `void(*glUniform3uiv)(SbGlInt32 location, SbGlSizei count, const SbGlUInt32
    *value)`
*   `void(*glUniform4uiv)(SbGlInt32 location, SbGlSizei count, const SbGlUInt32
    *value)`
*   `void(*glClearBufferiv)(SbGlEnum buffer, SbGlInt32 drawbuffer, const
    SbGlInt32 *value)`
*   `void(*glClearBufferuiv)(SbGlEnum buffer, SbGlInt32 drawbuffer, const
    SbGlUInt32 *value)`
*   `void(*glClearBufferfv)(SbGlEnum buffer, SbGlInt32 drawbuffer, const
    SbGlFloat *value)`
*   `void(*glClearBufferfi)(SbGlEnum buffer, SbGlInt32 drawbuffer, SbGlFloat
    depth, SbGlInt32 stencil)`
*   `const SbGlUInt8 *(*glGetStringi)(SbGlEnum name, SbGlUInt32 index)`
*   `void(*glCopyBufferSubData)(SbGlEnum readTarget, SbGlEnum writeTarget,
    SbGlIntPtr readOffset, SbGlIntPtr writeOffset, SbGlSizeiPtr size)`
*   `void(*glGetUniformIndices)(SbGlUInt32 program, SbGlSizei uniformCount,
    const SbGlChar *const *uniformNames, SbGlUInt32 *uniformIndices)`
*   `void(*glGetActiveUniformsiv)(SbGlUInt32 program, SbGlSizei uniformCount,
    const SbGlUInt32 *uniformIndices, SbGlEnum pname, SbGlInt32 *params)`
*   `SbGlUInt32(*glGetUniformBlockIndex)(SbGlUInt32 program, const SbGlChar
    *uniformBlockName)`
*   `void(*glGetActiveUniformBlockiv)(SbGlUInt32 program, SbGlUInt32
    uniformBlockIndex, SbGlEnum pname, SbGlInt32 *params)`
*   `void(*glGetActiveUniformBlockName)(SbGlUInt32 program, SbGlUInt32
    uniformBlockIndex, SbGlSizei bufSize, SbGlSizei *length, SbGlChar
    *uniformBlockName)`
*   `void(*glUniformBlockBinding)(SbGlUInt32 program, SbGlUInt32
    uniformBlockIndex, SbGlUInt32 uniformBlockBinding)`
*   `void(*glDrawArraysInstanced)(SbGlEnum mode, SbGlInt32 first, SbGlSizei
    count, SbGlSizei instancecount)`
*   `void(*glDrawElementsInstanced)(SbGlEnum mode, SbGlSizei count, SbGlEnum
    type, const void *indices, SbGlSizei instancecount)`
*   `SbGlSync(*glFenceSync)(SbGlEnum condition, SbGlBitfield flags)`
*   `SbGlBoolean(*glIsSync)(SbGlSync sync)`
*   `void(*glDeleteSync)(SbGlSync sync)`
*   `SbGlEnum(*glClientWaitSync)(SbGlSync sync, SbGlBitfield flags, SbGlUInt64
    timeout)`
*   `void(*glWaitSync)(SbGlSync sync, SbGlBitfield flags, SbGlUInt64 timeout)`
*   `void(*glGetInteger64v)(SbGlEnum pname, SbGlInt64 *data)`
*   `void(*glGetSynciv)(SbGlSync sync, SbGlEnum pname, SbGlSizei bufSize,
    SbGlSizei *length, SbGlInt32 *values)`
*   `void(*glGetInteger64i_v)(SbGlEnum target, SbGlUInt32 index, SbGlInt64
    *data)`
*   `void(*glGetBufferParameteri64v)(SbGlEnum target, SbGlEnum pname, SbGlInt64
    *params)`
*   `void(*glGenSamplers)(SbGlSizei count, SbGlUInt32 *samplers)`
*   `void(*glDeleteSamplers)(SbGlSizei count, const SbGlUInt32 *samplers)`
*   `SbGlBoolean(*glIsSampler)(SbGlUInt32 sampler)`
*   `void(*glBindSampler)(SbGlUInt32 unit, SbGlUInt32 sampler)`
*   `void(*glSamplerParameteri)(SbGlUInt32 sampler, SbGlEnum pname, SbGlInt32
    param)`
*   `void(*glSamplerParameteriv)(SbGlUInt32 sampler, SbGlEnum pname, const
    SbGlInt32 *param)`
*   `void(*glSamplerParameterf)(SbGlUInt32 sampler, SbGlEnum pname, SbGlFloat
    param)`
*   `void(*glSamplerParameterfv)(SbGlUInt32 sampler, SbGlEnum pname, const
    SbGlFloat *param)`
*   `void(*glGetSamplerParameteriv)(SbGlUInt32 sampler, SbGlEnum pname,
    SbGlInt32 *params)`
*   `void(*glGetSamplerParameterfv)(SbGlUInt32 sampler, SbGlEnum pname,
    SbGlFloat *params)`
*   `void(*glVertexAttribDivisor)(SbGlUInt32 index, SbGlUInt32 divisor)`
*   `void(*glBindTransformFeedback)(SbGlEnum target, SbGlUInt32 id)`
*   `void(*glDeleteTransformFeedbacks)(SbGlSizei n, const SbGlUInt32 *ids)`
*   `void(*glGenTransformFeedbacks)(SbGlSizei n, SbGlUInt32 *ids)`
*   `SbGlBoolean(*glIsTransformFeedback)(SbGlUInt32 id)`
*   `void(*glPauseTransformFeedback)(void)`
*   `void(*glResumeTransformFeedback)(void)`
*   `void(*glGetProgramBinary)(SbGlUInt32 program, SbGlSizei bufSize, SbGlSizei
    *length, SbGlEnum *binaryFormat, void *binary)`
*   `void(*glProgramBinary)(SbGlUInt32 program, SbGlEnum binaryFormat, const
    void *binary, SbGlSizei length)`
*   `void(*glProgramParameteri)(SbGlUInt32 program, SbGlEnum pname, SbGlInt32
    value)`
*   `void(*glInvalidateFramebuffer)(SbGlEnum target, SbGlSizei numAttachments,
    const SbGlEnum *attachments)`
*   `void(*glInvalidateSubFramebuffer)(SbGlEnum target, SbGlSizei
    numAttachments, const SbGlEnum *attachments, SbGlInt32 x, SbGlInt32 y,
    SbGlSizei width, SbGlSizei height)`
*   `void(*glTexStorage2D)(SbGlEnum target, SbGlSizei levels, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height)`
*   `void(*glTexStorage3D)(SbGlEnum target, SbGlSizei levels, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height, SbGlSizei depth)`
*   `void(*glGetInternalformativ)(SbGlEnum target, SbGlEnum internalformat,
    SbGlEnum pname, SbGlSizei bufSize, SbGlInt32 *params)`
