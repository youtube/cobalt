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

*   ` glActiveTexture`

    void (\*glActiveTexture)(SbGlEnum texture);
*   ` glAttachShader`

    void (\*glAttachShader)(SbGlUInt32 program, SbGlUInt32 shader);
*   ` glBindAttribLocation`

    void (\*glBindAttribLocation)(SbGlUInt32 program, SbGlUInt32 index, const
    SbGlChar\* name);
*   ` glBindBuffer`

    void (\*glBindBuffer)(SbGlEnum target, SbGlUInt32 buffer);
*   ` glBindFramebuffer`

    void (\*glBindFramebuffer)(SbGlEnum target, SbGlUInt32 framebuffer);
*   ` glBindRenderbuffer`

    void (\*glBindRenderbuffer)(SbGlEnum target, SbGlUInt32 renderbuffer);
*   ` glBindTexture`

    void (\*glBindTexture)(SbGlEnum target, SbGlUInt32 texture);
*   ` glBlendColor`

    void (\*glBlendColor)(SbGlFloat red, SbGlFloat green, SbGlFloat blue,
    SbGlFloat alpha);
*   ` glBlendEquation`

    void (\*glBlendEquation)(SbGlEnum mode);
*   ` glBlendEquationSeparate`

    void (\*glBlendEquationSeparate)(SbGlEnum modeRGB, SbGlEnum modeAlpha);
*   ` glBlendFunc`

    void (\*glBlendFunc)(SbGlEnum sfactor, SbGlEnum dfactor);
*   ` glBlendFuncSeparate`

    void (\*glBlendFuncSeparate)(SbGlEnum sfactorRGB, SbGlEnum dfactorRGB,
    SbGlEnum sfactorAlpha, SbGlEnum dfactorAlpha);
*   ` glBufferData`

    void (\*glBufferData)(SbGlEnum target, SbGlSizeiPtr size, const void\* data,
    SbGlEnum usage);
*   ` glBufferSubData`

    void (\*glBufferSubData)(SbGlEnum target, SbGlIntPtr offset, SbGlSizeiPtr
    size, const void\* data);
*   ` glCheckFramebufferStatus`

    SbGlEnum (\*glCheckFramebufferStatus)(SbGlEnum target);
*   ` glClear`

    void (\*glClear)(SbGlBitfield mask);
*   ` glClearColor`

    void (\*glClearColor)(SbGlFloat red, SbGlFloat green, SbGlFloat blue,
    SbGlFloat alpha);
*   ` glClearDepthf`

    void (\*glClearDepthf)(SbGlFloat d);
*   ` glClearStencil`

    void (\*glClearStencil)(SbGlInt32 s);
*   ` glColorMask`

    void (\*glColorMask)(SbGlBoolean red, SbGlBoolean green, SbGlBoolean blue,
    SbGlBoolean alpha);
*   ` glCompileShader`

    void (\*glCompileShader)(SbGlUInt32 shader);
*   ` glCompressedTexImage2D`

    void (\*glCompressedTexImage2D)(SbGlEnum target, SbGlInt32 level, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height, SbGlInt32 border,
    SbGlSizei imageSize, const void\* data);
*   ` glCompressedTexSubImage2D`

    void (\*glCompressedTexSubImage2D)(SbGlEnum target, SbGlInt32 level,
    SbGlInt32 xoffset, SbGlInt32 yoffset, SbGlSizei width, SbGlSizei height,
    SbGlEnum format, SbGlSizei imageSize, const void\* data);
*   ` glCopyTexImage2D`

    void (\*glCopyTexImage2D)(SbGlEnum target, SbGlInt32 level, SbGlEnum
    internalformat, SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei height,
    SbGlInt32 border);
*   ` glCopyTexSubImage2D`

    void (\*glCopyTexSubImage2D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    xoffset, SbGlInt32 yoffset, SbGlInt32 x, SbGlInt32 y, SbGlSizei width,
    SbGlSizei height);
*   ` glCreateProgram`

    SbGlUInt32 (\*glCreateProgram)(void);
*   ` glCreateShader`

    SbGlUInt32 (\*glCreateShader)(SbGlEnum type);
*   ` glCullFace`

    void (\*glCullFace)(SbGlEnum mode);
*   ` glDeleteBuffers`

    void (\*glDeleteBuffers)(SbGlSizei n, const SbGlUInt32\* buffers);
*   ` glDeleteFramebuffers`

    void (\*glDeleteFramebuffers)(SbGlSizei n, const SbGlUInt32\* framebuffers);
*   ` glDeleteProgram`

    void (\*glDeleteProgram)(SbGlUInt32 program);
*   ` glDeleteRenderbuffers`

    void (\*glDeleteRenderbuffers)(SbGlSizei n, const SbGlUInt32\*
    renderbuffers);
*   ` glDeleteShader`

    void (\*glDeleteShader)(SbGlUInt32 shader);
*   ` glDeleteTextures`

    void (\*glDeleteTextures)(SbGlSizei n, const SbGlUInt32\* textures);
*   ` glDepthFunc`

    void (\*glDepthFunc)(SbGlEnum func);
*   ` glDepthMask`

    void (\*glDepthMask)(SbGlBoolean flag);
*   ` glDepthRangef`

    void (\*glDepthRangef)(SbGlFloat n, SbGlFloat f);
*   ` glDetachShader`

    void (\*glDetachShader)(SbGlUInt32 program, SbGlUInt32 shader);
*   ` glDisable`

    void (\*glDisable)(SbGlEnum cap);
*   ` glDisableVertexAttribArray`

    void (\*glDisableVertexAttribArray)(SbGlUInt32 index);
*   ` glDrawArrays`

    void (\*glDrawArrays)(SbGlEnum mode, SbGlInt32 first, SbGlSizei count);
*   ` glDrawElements`

    void (\*glDrawElements)(SbGlEnum mode, SbGlSizei count, SbGlEnum type, const
    void\* indices);
*   ` glEnable`

    void (\*glEnable)(SbGlEnum cap);
*   ` glEnableVertexAttribArray`

    void (\*glEnableVertexAttribArray)(SbGlUInt32 index);
*   ` glFinish`

    void (\*glFinish)(void);
*   ` glFlush`

    void (\*glFlush)(void);
*   ` glFramebufferRenderbuffer`

    void (\*glFramebufferRenderbuffer)(SbGlEnum target, SbGlEnum attachment,
    SbGlEnum renderbuffertarget, SbGlUInt32 renderbuffer);
*   ` glFramebufferTexture2D`

    void (\*glFramebufferTexture2D)(SbGlEnum target, SbGlEnum attachment,
    SbGlEnum textarget, SbGlUInt32 texture, SbGlInt32 level);
*   ` glFrontFace`

    void (\*glFrontFace)(SbGlEnum mode);
*   ` glGenBuffers`

    void (\*glGenBuffers)(SbGlSizei n, SbGlUInt32\* buffers);
*   ` glGenerateMipmap`

    void (\*glGenerateMipmap)(SbGlEnum target);
*   ` glGenFramebuffers`

    void (\*glGenFramebuffers)(SbGlSizei n, SbGlUInt32\* framebuffers);
*   ` glGenRenderbuffers`

    void (\*glGenRenderbuffers)(SbGlSizei n, SbGlUInt32\* renderbuffers);
*   ` glGenTextures`

    void (\*glGenTextures)(SbGlSizei n, SbGlUInt32\* textures);
*   ` glGetActiveAttrib`

    void (\*glGetActiveAttrib)(SbGlUInt32 program, SbGlUInt32 index, SbGlSizei
    bufSize, SbGlSizei\* length, SbGlInt32\* size, SbGlEnum\* type, SbGlChar\*
    name);
*   ` glGetActiveUniform`

    void (\*glGetActiveUniform)(SbGlUInt32 program, SbGlUInt32 index, SbGlSizei
    bufSize, SbGlSizei\* length, SbGlInt32\* size, SbGlEnum\* type, SbGlChar\*
    name);
*   ` glGetAttachedShaders`

    void (\*glGetAttachedShaders)(SbGlUInt32 program, SbGlSizei maxCount,
    SbGlSizei\* count, SbGlUInt32\* shaders);
*   ` glGetAttribLocation`

    SbGlInt32 (\*glGetAttribLocation)(SbGlUInt32 program, const SbGlChar\*
    name);
*   ` glGetBooleanv`

    void (\*glGetBooleanv)(SbGlEnum pname, SbGlBoolean\* data);
*   ` glGetBufferParameteriv`

    void (\*glGetBufferParameteriv)(SbGlEnum target, SbGlEnum pname, SbGlInt32\*
    params);
*   ` glGetError`

    SbGlEnum (\*glGetError)(void);
*   ` glGetFloatv`

    void (\*glGetFloatv)(SbGlEnum pname, SbGlFloat\* data);
*   ` glGetFramebufferAttachmentParameteriv`

    void (\*glGetFramebufferAttachmentParameteriv)(SbGlEnum target, SbGlEnum
    attachment, SbGlEnum pname, SbGlInt32\* params);
*   ` glGetIntegerv`

    void (\*glGetIntegerv)(SbGlEnum pname, SbGlInt32\* data);
*   ` glGetProgramiv`

    void (\*glGetProgramiv)(SbGlUInt32 program, SbGlEnum pname, SbGlInt32\*
    params);
*   ` glGetProgramInfoLog`

    void (\*glGetProgramInfoLog)(SbGlUInt32 program, SbGlSizei bufSize,
    SbGlSizei\* length, SbGlChar\* infoLog);
*   ` glGetRenderbufferParameteriv`

    void (\*glGetRenderbufferParameteriv)(SbGlEnum target, SbGlEnum pname,
    SbGlInt32\* params);
*   ` glGetShaderiv`

    void (\*glGetShaderiv)(SbGlUInt32 shader, SbGlEnum pname, SbGlInt32\*
    params);
*   ` glGetShaderInfoLog`

    void (\*glGetShaderInfoLog)(SbGlUInt32 shader, SbGlSizei bufSize,
    SbGlSizei\* length, SbGlChar\* infoLog);
*   ` glGetShaderPrecisionFormat`

    void (\*glGetShaderPrecisionFormat)(SbGlEnum shadertype, SbGlEnum
    precisiontype, SbGlInt32\* range, SbGlInt32\* precision);
*   ` glGetShaderSource`

    void (\*glGetShaderSource)(SbGlUInt32 shader, SbGlSizei bufSize, SbGlSizei\*
    length, SbGlChar\* source);
*   ` glGetString`

    const SbGlUInt8\* (\*glGetString)(SbGlEnum name);
*   ` glGetTexParameterfv`

    void (\*glGetTexParameterfv)(SbGlEnum target, SbGlEnum pname, SbGlFloat\*
    params);
*   ` glGetTexParameteriv`

    void (\*glGetTexParameteriv)(SbGlEnum target, SbGlEnum pname, SbGlInt32\*
    params);
*   ` glGetUniformfv`

    void (\*glGetUniformfv)(SbGlUInt32 program, SbGlInt32 location, SbGlFloat\*
    params);
*   ` glGetUniformiv`

    void (\*glGetUniformiv)(SbGlUInt32 program, SbGlInt32 location, SbGlInt32\*
    params);
*   ` glGetUniformLocation`

    SbGlInt32 (\*glGetUniformLocation)(SbGlUInt32 program, const SbGlChar\*
    name);
*   ` glGetVertexAttribfv`

    void (\*glGetVertexAttribfv)(SbGlUInt32 index, SbGlEnum pname, SbGlFloat\*
    params);
*   ` glGetVertexAttribiv`

    void (\*glGetVertexAttribiv)(SbGlUInt32 index, SbGlEnum pname, SbGlInt32\*
    params);
*   ` glGetVertexAttribPointerv`

    void (\*glGetVertexAttribPointerv)(SbGlUInt32 index, SbGlEnum pname,
    void\*\* pointer);
*   ` glHint`

    void (\*glHint)(SbGlEnum target, SbGlEnum mode);
*   ` glIsBuffer`

    SbGlBoolean (\*glIsBuffer)(SbGlUInt32 buffer);
*   ` glIsEnabled`

    SbGlBoolean (\*glIsEnabled)(SbGlEnum cap);
*   ` glIsFramebuffer`

    SbGlBoolean (\*glIsFramebuffer)(SbGlUInt32 framebuffer);
*   ` glIsProgram`

    SbGlBoolean (\*glIsProgram)(SbGlUInt32 program);
*   ` glIsRenderbuffer`

    SbGlBoolean (\*glIsRenderbuffer)(SbGlUInt32 renderbuffer);
*   ` glIsShader`

    SbGlBoolean (\*glIsShader)(SbGlUInt32 shader);
*   ` glIsTexture`

    SbGlBoolean (\*glIsTexture)(SbGlUInt32 texture);
*   ` glLineWidth`

    void (\*glLineWidth)(SbGlFloat width);
*   ` glLinkProgram`

    void (\*glLinkProgram)(SbGlUInt32 program);
*   ` glPixelStorei`

    void (\*glPixelStorei)(SbGlEnum pname, SbGlInt32 param);
*   ` glPolygonOffset`

    void (\*glPolygonOffset)(SbGlFloat factor, SbGlFloat units);
*   ` glReadPixels`

    void (\*glReadPixels)(SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei
    height, SbGlEnum format, SbGlEnum type, void\* pixels);
*   ` glReleaseShaderCompiler`

    void (\*glReleaseShaderCompiler)(void);
*   ` glRenderbufferStorage`

    void (\*glRenderbufferStorage)(SbGlEnum target, SbGlEnum internalformat,
    SbGlSizei width, SbGlSizei height);
*   ` glSampleCoverage`

    void (\*glSampleCoverage)(SbGlFloat value, SbGlBoolean invert);
*   ` glScissor`

    void (\*glScissor)(SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei
    height);
*   ` glShaderBinary`

    void (\*glShaderBinary)(SbGlSizei count, const SbGlUInt32\* shaders,
    SbGlEnum binaryformat, const void\* binary, SbGlSizei length);
*   ` glShaderSource`

    void (\*glShaderSource)(SbGlUInt32 shader, SbGlSizei count, const SbGlChar\*
    const\* string, const SbGlInt32\* length);
*   ` glStencilFunc`

    void (\*glStencilFunc)(SbGlEnum func, SbGlInt32 ref, SbGlUInt32 mask);
*   ` glStencilFuncSeparate`

    void (\*glStencilFuncSeparate)(SbGlEnum face, SbGlEnum func, SbGlInt32 ref,
    SbGlUInt32 mask);
*   ` glStencilMask`

    void (\*glStencilMask)(SbGlUInt32 mask);
*   ` glStencilMaskSeparate`

    void (\*glStencilMaskSeparate)(SbGlEnum face, SbGlUInt32 mask);
*   ` glStencilOp`

    void (\*glStencilOp)(SbGlEnum fail, SbGlEnum zfail, SbGlEnum zpass);
*   ` glStencilOpSeparate`

    void (\*glStencilOpSeparate)(SbGlEnum face, SbGlEnum sfail, SbGlEnum dpfail,
    SbGlEnum dppass);
*   ` glTexImage2D`

    void (\*glTexImage2D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    internalformat, SbGlSizei width, SbGlSizei height, SbGlInt32 border,
    SbGlEnum format, SbGlEnum type, const void\* pixels);
*   ` glTexParameterf`

    void (\*glTexParameterf)(SbGlEnum target, SbGlEnum pname, SbGlFloat param);
*   ` glTexParameterfv`

    void (\*glTexParameterfv)(SbGlEnum target, SbGlEnum pname, const SbGlFloat\*
    params);
*   ` glTexParameteri`

    void (\*glTexParameteri)(SbGlEnum target, SbGlEnum pname, SbGlInt32 param);
*   ` glTexParameteriv`

    void (\*glTexParameteriv)(SbGlEnum target, SbGlEnum pname, const SbGlInt32\*
    params);
*   ` glTexSubImage2D`

    void (\*glTexSubImage2D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    xoffset, SbGlInt32 yoffset, SbGlSizei width, SbGlSizei height, SbGlEnum
    format, SbGlEnum type, const void\* pixels);
*   ` glUniform1f`

    void (\*glUniform1f)(SbGlInt32 location, SbGlFloat v0);
*   ` glUniform1fv`

    void (\*glUniform1fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat\*
    value);
*   ` glUniform1i`

    void (\*glUniform1i)(SbGlInt32 location, SbGlInt32 v0);
*   ` glUniform1iv`

    void (\*glUniform1iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32\*
    value);
*   ` glUniform2f`

    void (\*glUniform2f)(SbGlInt32 location, SbGlFloat v0, SbGlFloat v1);
*   ` glUniform2fv`

    void (\*glUniform2fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat\*
    value);
*   ` glUniform2i`

    void (\*glUniform2i)(SbGlInt32 location, SbGlInt32 v0, SbGlInt32 v1);
*   ` glUniform2iv`

    void (\*glUniform2iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32\*
    value);
*   ` glUniform3f`

    void (\*glUniform3f)(SbGlInt32 location, SbGlFloat v0, SbGlFloat v1,
    SbGlFloat v2);
*   ` glUniform3fv`

    void (\*glUniform3fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat\*
    value);
*   ` glUniform3i`

    void (\*glUniform3i)(SbGlInt32 location, SbGlInt32 v0, SbGlInt32 v1,
    SbGlInt32 v2);
*   ` glUniform3iv`

    void (\*glUniform3iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32\*
    value);
*   ` glUniform4f`

    void (\*glUniform4f)(SbGlInt32 location, SbGlFloat v0, SbGlFloat v1,
    SbGlFloat v2, SbGlFloat v3);
*   ` glUniform4fv`

    void (\*glUniform4fv)(SbGlInt32 location, SbGlSizei count, const SbGlFloat\*
    value);
*   ` glUniform4i`

    void (\*glUniform4i)(SbGlInt32 location, SbGlInt32 v0, SbGlInt32 v1,
    SbGlInt32 v2, SbGlInt32 v3);
*   ` glUniform4iv`

    void (\*glUniform4iv)(SbGlInt32 location, SbGlSizei count, const SbGlInt32\*
    value);
*   ` glUniformMatrix2fv`

    void (\*glUniformMatrix2fv)(SbGlInt32 location, SbGlSizei count, SbGlBoolean
    transpose, const SbGlFloat\* value);
*   ` glUniformMatrix3fv`

    void (\*glUniformMatrix3fv)(SbGlInt32 location, SbGlSizei count, SbGlBoolean
    transpose, const SbGlFloat\* value);
*   ` glUniformMatrix4fv`

    void (\*glUniformMatrix4fv)(SbGlInt32 location, SbGlSizei count, SbGlBoolean
    transpose, const SbGlFloat\* value);
*   ` glUseProgram`

    void (\*glUseProgram)(SbGlUInt32 program);
*   ` glValidateProgram`

    void (\*glValidateProgram)(SbGlUInt32 program);
*   ` glVertexAttrib1f`

    void (\*glVertexAttrib1f)(SbGlUInt32 index, SbGlFloat x);
*   ` glVertexAttrib1fv`

    void (\*glVertexAttrib1fv)(SbGlUInt32 index, const SbGlFloat\* v);
*   ` glVertexAttrib2f`

    void (\*glVertexAttrib2f)(SbGlUInt32 index, SbGlFloat x, SbGlFloat y);
*   ` glVertexAttrib2fv`

    void (\*glVertexAttrib2fv)(SbGlUInt32 index, const SbGlFloat\* v);
*   ` glVertexAttrib3f`

    void (\*glVertexAttrib3f)(SbGlUInt32 index, SbGlFloat x, SbGlFloat y,
    SbGlFloat z);
*   ` glVertexAttrib3fv`

    void (\*glVertexAttrib3fv)(SbGlUInt32 index, const SbGlFloat\* v);
*   ` glVertexAttrib4f`

    void (\*glVertexAttrib4f)(SbGlUInt32 index, SbGlFloat x, SbGlFloat y,
    SbGlFloat z, SbGlFloat w);
*   ` glVertexAttrib4fv`

    void (\*glVertexAttrib4fv)(SbGlUInt32 index, const SbGlFloat\* v);
*   ` glVertexAttribPointer`

    void (\*glVertexAttribPointer)(SbGlUInt32 index, SbGlInt32 size, SbGlEnum
    type, SbGlBoolean normalized, SbGlSizei stride, const void\* pointer);
*   ` glViewport`

    void (\*glViewport)(SbGlInt32 x, SbGlInt32 y, SbGlSizei width, SbGlSizei
    height);
*   ` glReadBuffer`

    void (\*glReadBuffer)(SbGlEnum src);
*   ` glDrawRangeElements`

    void (\*glDrawRangeElements)(SbGlEnum mode, SbGlUInt32 start, SbGlUInt32
    end, SbGlSizei count, SbGlEnum type, const void\* indices);
*   ` glTexImage3D`

    void (\*glTexImage3D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    internalformat, SbGlSizei width, SbGlSizei height, SbGlSizei depth,
    SbGlInt32 border, SbGlEnum format, SbGlEnum type, const void\* pixels);
*   ` glTexSubImage3D`

    void (\*glTexSubImage3D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    xoffset, SbGlInt32 yoffset, SbGlInt32 zoffset, SbGlSizei width, SbGlSizei
    height, SbGlSizei depth, SbGlEnum format, SbGlEnum type, const void\*
    pixels);
*   ` glCopyTexSubImage3D`

    void (\*glCopyTexSubImage3D)(SbGlEnum target, SbGlInt32 level, SbGlInt32
    xoffset, SbGlInt32 yoffset, SbGlInt32 zoffset, SbGlInt32 x, SbGlInt32 y,
    SbGlSizei width, SbGlSizei height);
*   ` glCompressedTexImage3D`

    void (\*glCompressedTexImage3D)(SbGlEnum target, SbGlInt32 level, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height, SbGlSizei depth,
    SbGlInt32 border, SbGlSizei imageSize, const void\* data);
*   ` glCompressedTexSubImage3D`

    void (\*glCompressedTexSubImage3D)(SbGlEnum target, SbGlInt32 level,
    SbGlInt32 xoffset, SbGlInt32 yoffset, SbGlInt32 zoffset, SbGlSizei width,
    SbGlSizei height, SbGlSizei depth, SbGlEnum format, SbGlSizei imageSize,
    const void\* data);
*   ` glGenQueries`

    void (\*glGenQueries)(SbGlSizei n, SbGlUInt32\* ids);
*   ` glDeleteQueries`

    void (\*glDeleteQueries)(SbGlSizei n, const SbGlUInt32\* ids);
*   ` glIsQuery`

    SbGlBoolean (\*glIsQuery)(SbGlUInt32 id);
*   ` glBeginQuery`

    void (\*glBeginQuery)(SbGlEnum target, SbGlUInt32 id);
*   ` glEndQuery`

    void (\*glEndQuery)(SbGlEnum target);
*   ` glGetQueryiv`

    void (\*glGetQueryiv)(SbGlEnum target, SbGlEnum pname, SbGlInt32\* params);
*   ` glGetQueryObjectuiv`

    void (\*glGetQueryObjectuiv)(SbGlUInt32 id, SbGlEnum pname, SbGlUInt32\*
    params);
*   ` glUnmapBuffer`

    SbGlBoolean (\*glUnmapBuffer)(SbGlEnum target);
*   ` glGetBufferPointerv`

    void (\*glGetBufferPointerv)(SbGlEnum target, SbGlEnum pname, void\*\*
    params);
*   ` glDrawBuffers`

    void (\*glDrawBuffers)(SbGlSizei n, const SbGlEnum\* bufs);
*   ` glUniformMatrix2x3fv`

    void (\*glUniformMatrix2x3fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat\* value);
*   ` glUniformMatrix3x2fv`

    void (\*glUniformMatrix3x2fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat\* value);
*   ` glUniformMatrix2x4fv`

    void (\*glUniformMatrix2x4fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat\* value);
*   ` glUniformMatrix4x2fv`

    void (\*glUniformMatrix4x2fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat\* value);
*   ` glUniformMatrix3x4fv`

    void (\*glUniformMatrix3x4fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat\* value);
*   ` glUniformMatrix4x3fv`

    void (\*glUniformMatrix4x3fv)(SbGlInt32 location, SbGlSizei count,
    SbGlBoolean transpose, const SbGlFloat\* value);
*   ` glBlitFramebuffer`

    void (\*glBlitFramebuffer)(SbGlInt32 srcX0, SbGlInt32 srcY0, SbGlInt32
    srcX1, SbGlInt32 srcY1, SbGlInt32 dstX0, SbGlInt32 dstY0, SbGlInt32 dstX1,
    SbGlInt32 dstY1, SbGlBitfield mask, SbGlEnum filter);
*   ` glRenderbufferStorageMultisample`

    void (\*glRenderbufferStorageMultisample)(SbGlEnum target, SbGlSizei
    samples, SbGlEnum internalformat, SbGlSizei width, SbGlSizei height);
*   ` glFramebufferTextureLayer`

    void (\*glFramebufferTextureLayer)(SbGlEnum target, SbGlEnum attachment,
    SbGlUInt32 texture, SbGlInt32 level, SbGlInt32 layer);
*   ` glMapBufferRange`

    void\* (\*glMapBufferRange)(SbGlEnum target, SbGlIntPtr offset, SbGlSizeiPtr
    length, SbGlBitfield access);
*   ` glFlushMappedBufferRange`

    void (\*glFlushMappedBufferRange)(SbGlEnum target, SbGlIntPtr offset,
    SbGlSizeiPtr length);
*   ` glBindVertexArray`

    void (\*glBindVertexArray)(SbGlUInt32 array);
*   ` glDeleteVertexArrays`

    void (\*glDeleteVertexArrays)(SbGlSizei n, const SbGlUInt32\* arrays);
*   ` glGenVertexArrays`

    void (\*glGenVertexArrays)(SbGlSizei n, SbGlUInt32\* arrays);
*   ` glIsVertexArray`

    SbGlBoolean (\*glIsVertexArray)(SbGlUInt32 array);
*   ` glGetIntegeri_v`

    void (\*glGetIntegeri_v)(SbGlEnum target, SbGlUInt32 index, SbGlInt32\*
    data);
*   ` glBeginTransformFeedback`

    void (\*glBeginTransformFeedback)(SbGlEnum primitiveMode);
*   ` glEndTransformFeedback`

    void (\*glEndTransformFeedback)(void);
*   ` glBindBufferRange`

    void (\*glBindBufferRange)(SbGlEnum target, SbGlUInt32 index, SbGlUInt32
    buffer, SbGlIntPtr offset, SbGlSizeiPtr size);
*   ` glBindBufferBase`

    void (\*glBindBufferBase)(SbGlEnum target, SbGlUInt32 index, SbGlUInt32
    buffer);
*   ` glTransformFeedbackVaryings`

    void (\*glTransformFeedbackVaryings)(SbGlUInt32 program, SbGlSizei count,
    const SbGlChar\* const\* varyings, SbGlEnum bufferMode);
*   ` glGetTransformFeedbackVarying`

    void (\*glGetTransformFeedbackVarying)(SbGlUInt32 program, SbGlUInt32 index,
    SbGlSizei bufSize, SbGlSizei\* length, SbGlSizei\* size, SbGlEnum\* type,
    SbGlChar\* name);
*   ` glVertexAttribIPointer`

    void (\*glVertexAttribIPointer)(SbGlUInt32 index, SbGlInt32 size, SbGlEnum
    type, SbGlSizei stride, const void\* pointer);
*   ` glGetVertexAttribIiv`

    void (\*glGetVertexAttribIiv)(SbGlUInt32 index, SbGlEnum pname, SbGlInt32\*
    params);
*   ` glGetVertexAttribIuiv`

    void (\*glGetVertexAttribIuiv)(SbGlUInt32 index, SbGlEnum pname,
    SbGlUInt32\* params);
*   ` glVertexAttribI4i`

    void (\*glVertexAttribI4i)(SbGlUInt32 index, SbGlInt32 x, SbGlInt32 y,
    SbGlInt32 z, SbGlInt32 w);
*   ` glVertexAttribI4ui`

    void (\*glVertexAttribI4ui)(SbGlUInt32 index, SbGlUInt32 x, SbGlUInt32 y,
    SbGlUInt32 z, SbGlUInt32 w);
*   ` glVertexAttribI4iv`

    void (\*glVertexAttribI4iv)(SbGlUInt32 index, const SbGlInt32\* v);
*   ` glVertexAttribI4uiv`

    void (\*glVertexAttribI4uiv)(SbGlUInt32 index, const SbGlUInt32\* v);
*   ` glGetUniformuiv`

    void (\*glGetUniformuiv)(SbGlUInt32 program, SbGlInt32 location,
    SbGlUInt32\* params);
*   ` glGetFragDataLocation`

    SbGlInt32 (\*glGetFragDataLocation)(SbGlUInt32 program, const SbGlChar\*
    name);
*   ` glUniform1ui`

    void (\*glUniform1ui)(SbGlInt32 location, SbGlUInt32 v0);
*   ` glUniform2ui`

    void (\*glUniform2ui)(SbGlInt32 location, SbGlUInt32 v0, SbGlUInt32 v1);
*   ` glUniform3ui`

    void (\*glUniform3ui)(SbGlInt32 location, SbGlUInt32 v0, SbGlUInt32 v1,
    SbGlUInt32 v2);
*   ` glUniform4ui`

    void (\*glUniform4ui)(SbGlInt32 location, SbGlUInt32 v0, SbGlUInt32 v1,
    SbGlUInt32 v2, SbGlUInt32 v3);
*   ` glUniform1uiv`

    void (\*glUniform1uiv)(SbGlInt32 location, SbGlSizei count, const
    SbGlUInt32\* value);
*   ` glUniform2uiv`

    void (\*glUniform2uiv)(SbGlInt32 location, SbGlSizei count, const
    SbGlUInt32\* value);
*   ` glUniform3uiv`

    void (\*glUniform3uiv)(SbGlInt32 location, SbGlSizei count, const
    SbGlUInt32\* value);
*   ` glUniform4uiv`

    void (\*glUniform4uiv)(SbGlInt32 location, SbGlSizei count, const
    SbGlUInt32\* value);
*   ` glClearBufferiv`

    void (\*glClearBufferiv)(SbGlEnum buffer, SbGlInt32 drawbuffer, const
    SbGlInt32\* value);
*   ` glClearBufferuiv`

    void (\*glClearBufferuiv)(SbGlEnum buffer, SbGlInt32 drawbuffer, const
    SbGlUInt32\* value);
*   ` glClearBufferfv`

    void (\*glClearBufferfv)(SbGlEnum buffer, SbGlInt32 drawbuffer, const
    SbGlFloat\* value);
*   ` glClearBufferfi`

    void (\*glClearBufferfi)(SbGlEnum buffer, SbGlInt32 drawbuffer, SbGlFloat
    depth, SbGlInt32 stencil);
*   ` glGetStringi`

    const SbGlUInt8\* (\*glGetStringi)(SbGlEnum name, SbGlUInt32 index);
*   ` glCopyBufferSubData`

    void (\*glCopyBufferSubData)(SbGlEnum readTarget, SbGlEnum writeTarget,
    SbGlIntPtr readOffset, SbGlIntPtr writeOffset, SbGlSizeiPtr size);
*   ` glGetUniformIndices`

    void (\*glGetUniformIndices)(SbGlUInt32 program, SbGlSizei uniformCount,
    const SbGlChar\* const\* uniformNames, SbGlUInt32\* uniformIndices);
*   ` glGetActiveUniformsiv`

    void (\*glGetActiveUniformsiv)(SbGlUInt32 program, SbGlSizei uniformCount,
    const SbGlUInt32\* uniformIndices, SbGlEnum pname, SbGlInt32\* params);
*   ` glGetUniformBlockIndex`

    SbGlUInt32 (\*glGetUniformBlockIndex)(SbGlUInt32 program, const SbGlChar\*
    uniformBlockName);
*   ` glGetActiveUniformBlockiv`

    void (\*glGetActiveUniformBlockiv)(SbGlUInt32 program, SbGlUInt32
    uniformBlockIndex, SbGlEnum pname, SbGlInt32\* params);
*   ` glGetActiveUniformBlockName`

    void (\*glGetActiveUniformBlockName)(SbGlUInt32 program, SbGlUInt32
    uniformBlockIndex, SbGlSizei bufSize, SbGlSizei\* length, SbGlChar\*
    uniformBlockName);
*   ` glUniformBlockBinding`

    void (\*glUniformBlockBinding)(SbGlUInt32 program, SbGlUInt32
    uniformBlockIndex, SbGlUInt32 uniformBlockBinding);
*   ` glDrawArraysInstanced`

    void (\*glDrawArraysInstanced)(SbGlEnum mode, SbGlInt32 first, SbGlSizei
    count, SbGlSizei instancecount);
*   ` glDrawElementsInstanced`

    void (\*glDrawElementsInstanced)(SbGlEnum mode, SbGlSizei count, SbGlEnum
    type, const void\* indices, SbGlSizei instancecount);
*   ` glFenceSync`

    SbGlSync (\*glFenceSync)(SbGlEnum condition, SbGlBitfield flags);
*   ` glIsSync`

    SbGlBoolean (\*glIsSync)(SbGlSync sync);
*   ` glDeleteSync`

    void (\*glDeleteSync)(SbGlSync sync);
*   ` glClientWaitSync`

    SbGlEnum (\*glClientWaitSync)(SbGlSync sync, SbGlBitfield flags, SbGlUInt64
    timeout);
*   ` glWaitSync`

    void (\*glWaitSync)(SbGlSync sync, SbGlBitfield flags, SbGlUInt64 timeout);
*   ` glGetInteger64v`

    void (\*glGetInteger64v)(SbGlEnum pname, SbGlInt64\* data);
*   ` glGetSynciv`

    void (\*glGetSynciv)(SbGlSync sync, SbGlEnum pname, SbGlSizei bufSize,
    SbGlSizei\* length, SbGlInt32\* values);
*   ` glGetInteger64i_v`

    void (\*glGetInteger64i_v)(SbGlEnum target, SbGlUInt32 index, SbGlInt64\*
    data);
*   ` glGetBufferParameteri64v`

    void (\*glGetBufferParameteri64v)(SbGlEnum target, SbGlEnum pname,
    SbGlInt64\* params);
*   ` glGenSamplers`

    void (\*glGenSamplers)(SbGlSizei count, SbGlUInt32\* samplers);
*   ` glDeleteSamplers`

    void (\*glDeleteSamplers)(SbGlSizei count, const SbGlUInt32\* samplers);
*   ` glIsSampler`

    SbGlBoolean (\*glIsSampler)(SbGlUInt32 sampler);
*   ` glBindSampler`

    void (\*glBindSampler)(SbGlUInt32 unit, SbGlUInt32 sampler);
*   ` glSamplerParameteri`

    void (\*glSamplerParameteri)(SbGlUInt32 sampler, SbGlEnum pname, SbGlInt32
    param);
*   ` glSamplerParameteriv`

    void (\*glSamplerParameteriv)(SbGlUInt32 sampler, SbGlEnum pname, const
    SbGlInt32\* param);
*   ` glSamplerParameterf`

    void (\*glSamplerParameterf)(SbGlUInt32 sampler, SbGlEnum pname, SbGlFloat
    param);
*   ` glSamplerParameterfv`

    void (\*glSamplerParameterfv)(SbGlUInt32 sampler, SbGlEnum pname, const
    SbGlFloat\* param);
*   ` glGetSamplerParameteriv`

    void (\*glGetSamplerParameteriv)(SbGlUInt32 sampler, SbGlEnum pname,
    SbGlInt32\* params);
*   ` glGetSamplerParameterfv`

    void (\*glGetSamplerParameterfv)(SbGlUInt32 sampler, SbGlEnum pname,
    SbGlFloat\* params);
*   ` glVertexAttribDivisor`

    void (\*glVertexAttribDivisor)(SbGlUInt32 index, SbGlUInt32 divisor);
*   ` glBindTransformFeedback`

    void (\*glBindTransformFeedback)(SbGlEnum target, SbGlUInt32 id);
*   ` glDeleteTransformFeedbacks`

    void (\*glDeleteTransformFeedbacks)(SbGlSizei n, const SbGlUInt32\* ids);
*   ` glGenTransformFeedbacks`

    void (\*glGenTransformFeedbacks)(SbGlSizei n, SbGlUInt32\* ids);
*   ` glIsTransformFeedback`

    SbGlBoolean (\*glIsTransformFeedback)(SbGlUInt32 id);
*   ` glPauseTransformFeedback`

    void (\*glPauseTransformFeedback)(void);
*   ` glResumeTransformFeedback`

    void (\*glResumeTransformFeedback)(void);
*   ` glGetProgramBinary`

    void (\*glGetProgramBinary)(SbGlUInt32 program, SbGlSizei bufSize,
    SbGlSizei\* length, SbGlEnum\* binaryFormat, void\* binary);
*   ` glProgramBinary`

    void (\*glProgramBinary)(SbGlUInt32 program, SbGlEnum binaryFormat, const
    void\* binary, SbGlSizei length);
*   ` glProgramParameteri`

    void (\*glProgramParameteri)(SbGlUInt32 program, SbGlEnum pname, SbGlInt32
    value);
*   ` glInvalidateFramebuffer`

    void (\*glInvalidateFramebuffer)(SbGlEnum target, SbGlSizei numAttachments,
    const SbGlEnum\* attachments);
*   ` glInvalidateSubFramebuffer`

    void (\*glInvalidateSubFramebuffer)(SbGlEnum target, SbGlSizei
    numAttachments, const SbGlEnum\* attachments, SbGlInt32 x, SbGlInt32 y,
    SbGlSizei width, SbGlSizei height);
*   ` glTexStorage2D`

    void (\*glTexStorage2D)(SbGlEnum target, SbGlSizei levels, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height);
*   ` glTexStorage3D`

    void (\*glTexStorage3D)(SbGlEnum target, SbGlSizei levels, SbGlEnum
    internalformat, SbGlSizei width, SbGlSizei height, SbGlSizei depth);
*   ` glGetInternalformativ`

    void (\*glGetInternalformativ)(SbGlEnum target, SbGlEnum internalformat,
    SbGlEnum pname, SbGlSizei bufSize, SbGlInt32\* params);
