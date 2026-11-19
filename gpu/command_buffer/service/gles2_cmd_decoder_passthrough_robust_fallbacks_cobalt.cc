// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough_robust_fallbacks_cobalt.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

#ifndef GL_TEXTURE_BORDER_COLOR_EXT
#define GL_TEXTURE_BORDER_COLOR_EXT 0x1004
#endif

namespace gpu {
namespace gles2 {

namespace {

// Scratch space for queries: the native entry points write as many values as
// the driver thinks |pname| has, which can be more than the client asked for.
constexpr GLsizei kMinScratchValues = 64;

const gl::ProcsGL& Fn() {
  return gl::g_current_gl_driver->fn;
}

bool IsES3() {
  return gl::g_current_gl_version && gl::g_current_gl_version->is_es3;
}

GLint GetInt(GLenum pname) {
  GLint value = 0;
  Fn().glGetIntegervFn(pname, &value);
  return value;
}

template <typename T>
T LookUp(const char* name) {
  return reinterpret_cast<T>(gl::GetGLProcAddress(name));
}

// Runs |query| into scratch memory and copies the first |count| values (at
// most |buf_size|) to |out|, reporting the number copied in |length|.
template <typename T, typename Query>
void QueryValues(GLsizei count,
                 GLsizei buf_size,
                 GLsizei* length,
                 T* out,
                 Query query) {
  if (length) {
    *length = 0;
  }
  count = std::max<GLsizei>(count, 1);
  std::vector<T> scratch(std::max(count, kMinScratchValues), T());
  query(scratch.data());
  GLsizei n = std::min(count, std::max<GLsizei>(buf_size, 0));
  if (out && n > 0) {
    std::copy_n(scratch.begin(), n, out);
  }
  if (length) {
    *length = n;
  }
}

// Number of values glGet{Integer,Boolean,Float,Integer64}v writes for |pname|.
GLsizei StateValueCount(GLenum pname) {
  switch (pname) {
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return GetInt(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
    case GL_SHADER_BINARY_FORMATS:
      return GetInt(GL_NUM_SHADER_BINARY_FORMATS);
    case GL_PROGRAM_BINARY_FORMATS:
      return GetInt(GL_NUM_PROGRAM_BINARY_FORMATS);
    default:
      break;
  }
  int count = GLES2Util().GLGetNumValuesReturned(pname);
  return count > 0 ? count : 1;
}

GLsizei TexParameterValueCount(GLenum pname) {
  return pname == GL_TEXTURE_BORDER_COLOR_EXT ? 4 : 1;
}

GLsizei VertexAttribValueCount(GLenum pname) {
  return pname == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1;
}

// Number of components of the uniform at |location| in |program|, or 0.
GLsizei UniformComponentCount(GLuint program, GLint location) {
  GLint num_uniforms = 0;
  GLint max_name_length = 0;
  Fn().glGetProgramivFn(program, GL_ACTIVE_UNIFORMS, &num_uniforms);
  Fn().glGetProgramivFn(program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                        &max_name_length);
  if (num_uniforms <= 0 || max_name_length <= 0) {
    return 0;
  }
  std::vector<char> name_buffer(max_name_length + 1, 0);
  for (GLint i = 0; i < num_uniforms; ++i) {
    GLsizei name_length = 0;
    GLint size = 0;
    GLenum type = 0;
    Fn().glGetActiveUniformFn(program, i, max_name_length, &name_length, &size,
                              &type, name_buffer.data());
    std::string name(name_buffer.data(), std::max<GLsizei>(name_length, 0));
    GLsizei count = GLES2Util::GetElementCountForUniformType(type);
    if (Fn().glGetUniformLocationFn(program, name.c_str()) == location) {
      return count;
    }
    if (size > 1) {
      std::string base = name;
      if (base.size() > 3 && base.compare(base.size() - 3, 3, "[0]") == 0) {
        base.resize(base.size() - 3);
      }
      for (GLint element = 1; element < size; ++element) {
        std::string element_name = base + "[" + std::to_string(element) + "]";
        if (Fn().glGetUniformLocationFn(program, element_name.c_str()) ==
            location) {
          return count;
        }
      }
    }
  }
  return 0;
}

// Size in bytes of |width| x |height| x |depth| pixels, honoring the current
// pack (|pack| = true) or unpack pixel store state. Returns false if the
// pixels come from or go to a bound pixel buffer (no client memory involved),
// or if the size can't be computed.
bool ClientImageSize(bool pack,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     uint32_t* size) {
  PixelStoreParams params;
  if (pack) {
    params.alignment = GetInt(GL_PACK_ALIGNMENT);
    if (IsES3()) {
      if (GetInt(GL_PIXEL_PACK_BUFFER_BINDING) != 0) {
        return false;
      }
      params.row_length = GetInt(GL_PACK_ROW_LENGTH);
      params.skip_pixels = GetInt(GL_PACK_SKIP_PIXELS);
      params.skip_rows = GetInt(GL_PACK_SKIP_ROWS);
    }
  } else {
    params.alignment = GetInt(GL_UNPACK_ALIGNMENT);
    if (IsES3()) {
      if (GetInt(GL_PIXEL_UNPACK_BUFFER_BINDING) != 0) {
        return false;
      }
      params.row_length = GetInt(GL_UNPACK_ROW_LENGTH);
      params.image_height = GetInt(GL_UNPACK_IMAGE_HEIGHT);
      params.skip_pixels = GetInt(GL_UNPACK_SKIP_PIXELS);
      params.skip_rows = GetInt(GL_UNPACK_SKIP_ROWS);
      params.skip_images = GetInt(GL_UNPACK_SKIP_IMAGES);
    }
  }
  return GLES2Util::ComputeImageDataSizesES3(width, height, depth, format, type,
                                             params, size, nullptr, nullptr,
                                             nullptr, nullptr);
}

// Whether |pixels| holds enough data for an upload of the given size. Pixel
// buffer uploads (offsets, not client memory) and null pixels are allowed.
bool UploadFits(GLsizei width,
                GLsizei height,
                GLsizei depth,
                GLenum format,
                GLenum type,
                GLsizei buf_size,
                const void* pixels) {
  uint32_t size = 0;
  if (!pixels ||
      !ClientImageSize(false, width, height, depth, format, type, &size)) {
    return true;
  }
  if (size > static_cast<uint32_t>(std::max<GLsizei>(buf_size, 0))) {
    LOG(ERROR) << "Robust upload fallback: " << size
               << " bytes needed, but only " << buf_size << " provided.";
    return false;
  }
  return true;
}

// ---- State queries.

void GL_BINDING_CALL GetBooleanvRobust(GLenum pname,
                                       GLsizei buf_size,
                                       GLsizei* length,
                                       GLboolean* data) {
  QueryValues(StateValueCount(pname), buf_size, length, data,
              [&](GLboolean* out) { Fn().glGetBooleanvFn(pname, out); });
}

void GL_BINDING_CALL GetFloatvRobust(GLenum pname,
                                     GLsizei buf_size,
                                     GLsizei* length,
                                     GLfloat* data) {
  QueryValues(StateValueCount(pname), buf_size, length, data,
              [&](GLfloat* out) { Fn().glGetFloatvFn(pname, out); });
}

void GL_BINDING_CALL GetIntegervRobust(GLenum pname,
                                       GLsizei buf_size,
                                       GLsizei* length,
                                       GLint* data) {
  QueryValues(StateValueCount(pname), buf_size, length, data,
              [&](GLint* out) { Fn().glGetIntegervFn(pname, out); });
}

void GL_BINDING_CALL GetInteger64vRobust(GLenum pname,
                                         GLsizei buf_size,
                                         GLsizei* length,
                                         GLint64* data) {
  if (!Fn().glGetInteger64vFn) {
    GLint value = 0;
    QueryValues(StateValueCount(pname), buf_size, length, data,
                [&](GLint64* out) {
                  Fn().glGetIntegervFn(pname, &value);
                  out[0] = value;
                });
    return;
  }
  QueryValues(StateValueCount(pname), buf_size, length, data,
              [&](GLint64* out) { Fn().glGetInteger64vFn(pname, out); });
}

void GL_BINDING_CALL GetBooleani_vRobust(GLenum target,
                                         GLuint index,
                                         GLsizei buf_size,
                                         GLsizei* length,
                                         GLboolean* data) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetBooleani_vFn) {
    return;
  }
  QueryValues(1, buf_size, length, data, [&](GLboolean* out) {
    Fn().glGetBooleani_vFn(target, index, out);
  });
}

void GL_BINDING_CALL GetIntegeri_vRobust(GLenum target,
                                         GLuint index,
                                         GLsizei buf_size,
                                         GLsizei* length,
                                         GLint* data) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetIntegeri_vFn) {
    return;
  }
  QueryValues(1, buf_size, length, data,
              [&](GLint* out) { Fn().glGetIntegeri_vFn(target, index, out); });
}

void GL_BINDING_CALL GetInteger64i_vRobust(GLenum target,
                                           GLuint index,
                                           GLsizei buf_size,
                                           GLsizei* length,
                                           GLint64* data) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetInteger64i_vFn) {
    return;
  }
  QueryValues(1, buf_size, length, data, [&](GLint64* out) {
    Fn().glGetInteger64i_vFn(target, index, out);
  });
}

void GL_BINDING_CALL GetPointervRobust(GLenum pname,
                                       GLsizei buf_size,
                                       GLsizei* length,
                                       void** params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetPointervFn) {
    return;
  }
  QueryValues(1, buf_size, length, params,
              [&](void** out) { Fn().glGetPointervFn(pname, out); });
}

void GL_BINDING_CALL GetInternalformativRobust(GLenum target,
                                               GLenum internalformat,
                                               GLenum pname,
                                               GLsizei buf_size,
                                               GLsizei* length,
                                               GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetInternalformativFn) {
    return;
  }
  GLsizei count = 1;
  if (pname == GL_SAMPLES) {
    GLint num_sample_counts = 0;
    Fn().glGetInternalformativFn(target, internalformat, GL_NUM_SAMPLE_COUNTS,
                                 1, &num_sample_counts);
    count = num_sample_counts;
    if (count <= 0) {
      return;
    }
  }
  QueryValues(count, buf_size, length, params, [&](GLint* out) {
    Fn().glGetInternalformativFn(target, internalformat, pname,
                                 std::max(count, kMinScratchValues), out);
  });
}

void GL_BINDING_CALL GetMultisamplefvRobust(GLenum pname,
                                            GLuint index,
                                            GLsizei buf_size,
                                            GLsizei* length,
                                            GLfloat* val) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetMultisamplefvFn) {
    return;
  }
  QueryValues(2, buf_size, length, val, [&](GLfloat* out) {
    Fn().glGetMultisamplefvFn(pname, index, out);
  });
}

// ---- Object queries.

void GL_BINDING_CALL GetActiveUniformBlockivRobust(GLuint program,
                                                   GLuint block_index,
                                                   GLenum pname,
                                                   GLsizei buf_size,
                                                   GLsizei* length,
                                                   GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetActiveUniformBlockivFn) {
    return;
  }
  GLsizei count = 1;
  if (pname == GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES) {
    GLint active_uniforms = 0;
    Fn().glGetActiveUniformBlockivFn(program, block_index,
                                     GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                                     &active_uniforms);
    count = active_uniforms;
    if (count <= 0) {
      return;
    }
  }
  QueryValues(count, buf_size, length, params, [&](GLint* out) {
    Fn().glGetActiveUniformBlockivFn(program, block_index, pname, out);
  });
}

void GL_BINDING_CALL GetBufferParameterivRobust(GLenum target,
                                                GLenum pname,
                                                GLsizei buf_size,
                                                GLsizei* length,
                                                GLint* params) {
  QueryValues(1, buf_size, length, params, [&](GLint* out) {
    Fn().glGetBufferParameterivFn(target, pname, out);
  });
}

using GetBufferParameteri64vProc = void(GL_BINDING_CALL*)(GLenum,
                                                          GLenum,
                                                          GLint64*);

void GL_BINDING_CALL GetBufferParameteri64vRobust(GLenum target,
                                                  GLenum pname,
                                                  GLsizei buf_size,
                                                  GLsizei* length,
                                                  GLint64* params) {
  static const GetBufferParameteri64vProc fn =
      LookUp<GetBufferParameteri64vProc>("glGetBufferParameteri64v");
  if (fn) {
    QueryValues(1, buf_size, length, params,
                [&](GLint64* out) { fn(target, pname, out); });
    return;
  }
  GLint value = 0;
  QueryValues(1, buf_size, length, params, [&](GLint64* out) {
    Fn().glGetBufferParameterivFn(target, pname, &value);
    out[0] = value;
  });
}

using GetBufferPointervProc = void(GL_BINDING_CALL*)(GLenum, GLenum, void**);

void GL_BINDING_CALL GetBufferPointervRobust(GLenum target,
                                             GLenum pname,
                                             GLsizei buf_size,
                                             GLsizei* length,
                                             void** params) {
  static const GetBufferPointervProc fn =
      LookUp<GetBufferPointervProc>("glGetBufferPointerv");
  if (length) {
    *length = 0;
  }
  if (!fn) {
    return;
  }
  QueryValues(1, buf_size, length, params,
              [&](void** out) { fn(target, pname, out); });
}

void GL_BINDING_CALL
GetFramebufferAttachmentParameterivRobust(GLenum target,
                                          GLenum attachment,
                                          GLenum pname,
                                          GLsizei buf_size,
                                          GLsizei* length,
                                          GLint* params) {
  QueryValues(1, buf_size, length, params, [&](GLint* out) {
    Fn().glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                    out);
  });
}

void GL_BINDING_CALL GetFramebufferParameterivRobust(GLenum target,
                                                     GLenum pname,
                                                     GLsizei buf_size,
                                                     GLsizei* length,
                                                     GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetFramebufferParameterivFn) {
    return;
  }
  QueryValues(1, buf_size, length, params, [&](GLint* out) {
    Fn().glGetFramebufferParameterivFn(target, pname, out);
  });
}

void GL_BINDING_CALL GetProgramInterfaceivRobust(GLuint program,
                                                 GLenum program_interface,
                                                 GLenum pname,
                                                 GLsizei buf_size,
                                                 GLsizei* length,
                                                 GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetProgramInterfaceivFn) {
    return;
  }
  QueryValues(1, buf_size, length, params, [&](GLint* out) {
    Fn().glGetProgramInterfaceivFn(program, program_interface, pname, out);
  });
}

void GL_BINDING_CALL GetProgramivRobust(GLuint program,
                                        GLenum pname,
                                        GLsizei buf_size,
                                        GLsizei* length,
                                        GLint* params) {
  GLsizei count = pname == GL_COMPUTE_WORK_GROUP_SIZE ? 3 : 1;
  QueryValues(count, buf_size, length, params,
              [&](GLint* out) { Fn().glGetProgramivFn(program, pname, out); });
}

void GL_BINDING_CALL GetShaderivRobust(GLuint shader,
                                       GLenum pname,
                                       GLsizei buf_size,
                                       GLsizei* length,
                                       GLint* params) {
  QueryValues(1, buf_size, length, params,
              [&](GLint* out) { Fn().glGetShaderivFn(shader, pname, out); });
}

void GL_BINDING_CALL GetQueryivRobust(GLenum target,
                                      GLenum pname,
                                      GLsizei buf_size,
                                      GLsizei* length,
                                      GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetQueryivFn) {
    return;
  }
  QueryValues(1, buf_size, length, params,
              [&](GLint* out) { Fn().glGetQueryivFn(target, pname, out); });
}

#define QUERY_OBJECT_FALLBACK(Suffix, Type)                       \
  void GL_BINDING_CALL GetQueryObject##Suffix##Robust(            \
      GLuint id, GLenum pname, GLsizei buf_size, GLsizei* length, \
      Type* params) {                                             \
    if (length) {                                                 \
      *length = 0;                                                \
    }                                                             \
    if (!Fn().glGetQueryObject##Suffix##Fn) {                     \
      return;                                                     \
    }                                                             \
    QueryValues(1, buf_size, length, params, [&](Type* out) {     \
      Fn().glGetQueryObject##Suffix##Fn(id, pname, out);          \
    });                                                           \
  }

QUERY_OBJECT_FALLBACK(iv, GLint)
QUERY_OBJECT_FALLBACK(uiv, GLuint)
QUERY_OBJECT_FALLBACK(i64v, GLint64)
QUERY_OBJECT_FALLBACK(ui64v, GLuint64)

#undef QUERY_OBJECT_FALLBACK

void GL_BINDING_CALL GetRenderbufferParameterivRobust(GLenum target,
                                                      GLenum pname,
                                                      GLsizei buf_size,
                                                      GLsizei* length,
                                                      GLint* params) {
  QueryValues(1, buf_size, length, params, [&](GLint* out) {
    Fn().glGetRenderbufferParameterivEXTFn(target, pname, out);
  });
}

void GL_BINDING_CALL GetSamplerParameterfvRobust(GLuint sampler,
                                                 GLenum pname,
                                                 GLsizei buf_size,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetSamplerParameterfvFn) {
    return;
  }
  QueryValues(TexParameterValueCount(pname), buf_size, length, params,
              [&](GLfloat* out) {
                Fn().glGetSamplerParameterfvFn(sampler, pname, out);
              });
}

void GL_BINDING_CALL GetSamplerParameterivRobust(GLuint sampler,
                                                 GLenum pname,
                                                 GLsizei buf_size,
                                                 GLsizei* length,
                                                 GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetSamplerParameterivFn) {
    return;
  }
  QueryValues(
      TexParameterValueCount(pname), buf_size, length, params,
      [&](GLint* out) { Fn().glGetSamplerParameterivFn(sampler, pname, out); });
}

#define LOOKED_UP_PARAMETER_QUERY_FALLBACK(Name, ObjectType, Type, Count, \
                                           Symbol)                        \
  using Name##Proc = void(GL_BINDING_CALL*)(ObjectType, GLenum, Type*);   \
  void GL_BINDING_CALL Name##Robust(ObjectType object, GLenum pname,      \
                                    GLsizei buf_size, GLsizei* length,    \
                                    Type* params) {                       \
    static const Name##Proc fn = LookUp<Name##Proc>(Symbol);              \
    if (length) {                                                         \
      *length = 0;                                                        \
    }                                                                     \
    if (!fn) {                                                            \
      return;                                                             \
    }                                                                     \
    QueryValues(Count(pname), buf_size, length, params,                   \
                [&](Type* out) { fn(object, pname, out); });              \
  }

LOOKED_UP_PARAMETER_QUERY_FALLBACK(GetSamplerParameterIiv,
                                   GLuint,
                                   GLint,
                                   TexParameterValueCount,
                                   "glGetSamplerParameterIiv")
LOOKED_UP_PARAMETER_QUERY_FALLBACK(GetSamplerParameterIuiv,
                                   GLuint,
                                   GLuint,
                                   TexParameterValueCount,
                                   "glGetSamplerParameterIuiv")
LOOKED_UP_PARAMETER_QUERY_FALLBACK(GetTexParameterIiv,
                                   GLenum,
                                   GLint,
                                   TexParameterValueCount,
                                   "glGetTexParameterIiv")
LOOKED_UP_PARAMETER_QUERY_FALLBACK(GetTexParameterIuiv,
                                   GLenum,
                                   GLuint,
                                   TexParameterValueCount,
                                   "glGetTexParameterIuiv")
LOOKED_UP_PARAMETER_QUERY_FALLBACK(GetVertexAttribIiv,
                                   GLuint,
                                   GLint,
                                   VertexAttribValueCount,
                                   "glGetVertexAttribIiv")
LOOKED_UP_PARAMETER_QUERY_FALLBACK(GetVertexAttribIuiv,
                                   GLuint,
                                   GLuint,
                                   VertexAttribValueCount,
                                   "glGetVertexAttribIuiv")

#undef LOOKED_UP_PARAMETER_QUERY_FALLBACK

void GL_BINDING_CALL GetTexParameterfvRobust(GLenum target,
                                             GLenum pname,
                                             GLsizei buf_size,
                                             GLsizei* length,
                                             GLfloat* params) {
  QueryValues(
      TexParameterValueCount(pname), buf_size, length, params,
      [&](GLfloat* out) { Fn().glGetTexParameterfvFn(target, pname, out); });
}

void GL_BINDING_CALL GetTexParameterivRobust(GLenum target,
                                             GLenum pname,
                                             GLsizei buf_size,
                                             GLsizei* length,
                                             GLint* params) {
  QueryValues(
      TexParameterValueCount(pname), buf_size, length, params,
      [&](GLint* out) { Fn().glGetTexParameterivFn(target, pname, out); });
}

void GL_BINDING_CALL GetTexLevelParameterfvRobust(GLenum target,
                                                  GLint level,
                                                  GLenum pname,
                                                  GLsizei buf_size,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetTexLevelParameterfvFn) {
    return;
  }
  QueryValues(1, buf_size, length, params, [&](GLfloat* out) {
    Fn().glGetTexLevelParameterfvFn(target, level, pname, out);
  });
}

void GL_BINDING_CALL GetTexLevelParameterivRobust(GLenum target,
                                                  GLint level,
                                                  GLenum pname,
                                                  GLsizei buf_size,
                                                  GLsizei* length,
                                                  GLint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetTexLevelParameterivFn) {
    return;
  }
  QueryValues(1, buf_size, length, params, [&](GLint* out) {
    Fn().glGetTexLevelParameterivFn(target, level, pname, out);
  });
}

void GL_BINDING_CALL GetVertexAttribfvRobust(GLuint index,
                                             GLenum pname,
                                             GLsizei buf_size,
                                             GLsizei* length,
                                             GLfloat* params) {
  QueryValues(
      VertexAttribValueCount(pname), buf_size, length, params,
      [&](GLfloat* out) { Fn().glGetVertexAttribfvFn(index, pname, out); });
}

void GL_BINDING_CALL GetVertexAttribivRobust(GLuint index,
                                             GLenum pname,
                                             GLsizei buf_size,
                                             GLsizei* length,
                                             GLint* params) {
  QueryValues(
      VertexAttribValueCount(pname), buf_size, length, params,
      [&](GLint* out) { Fn().glGetVertexAttribivFn(index, pname, out); });
}

void GL_BINDING_CALL GetVertexAttribPointervRobust(GLuint index,
                                                   GLenum pname,
                                                   GLsizei buf_size,
                                                   GLsizei* length,
                                                   void** pointer) {
  QueryValues(1, buf_size, length, pointer, [&](void** out) {
    Fn().glGetVertexAttribPointervFn(index, pname, out);
  });
}

// ---- Uniforms.

void GL_BINDING_CALL GetUniformfvRobust(GLuint program,
                                        GLint location,
                                        GLsizei buf_size,
                                        GLsizei* length,
                                        GLfloat* params) {
  QueryValues(
      UniformComponentCount(program, location), buf_size, length, params,
      [&](GLfloat* out) { Fn().glGetUniformfvFn(program, location, out); });
}

void GL_BINDING_CALL GetUniformivRobust(GLuint program,
                                        GLint location,
                                        GLsizei buf_size,
                                        GLsizei* length,
                                        GLint* params) {
  QueryValues(
      UniformComponentCount(program, location), buf_size, length, params,
      [&](GLint* out) { Fn().glGetUniformivFn(program, location, out); });
}

void GL_BINDING_CALL GetUniformuivRobust(GLuint program,
                                         GLint location,
                                         GLsizei buf_size,
                                         GLsizei* length,
                                         GLuint* params) {
  if (length) {
    *length = 0;
  }
  if (!Fn().glGetUniformuivFn) {
    return;
  }
  QueryValues(
      UniformComponentCount(program, location), buf_size, length, params,
      [&](GLuint* out) { Fn().glGetUniformuivFn(program, location, out); });
}

// ---- Pixels.

void GL_BINDING_CALL ReadPixelsRobust(GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei buf_size,
                                      GLsizei* length,
                                      GLsizei* columns,
                                      GLsizei* rows,
                                      void* pixels) {
  if (length) {
    *length = 0;
  }
  if (columns) {
    *columns = 0;
  }
  if (rows) {
    *rows = 0;
  }
  uint32_t size = 0;
  bool client_memory =
      ClientImageSize(true, width, height, 1, format, type, &size);
  if (client_memory &&
      size > static_cast<uint32_t>(std::max<GLsizei>(buf_size, 0))) {
    LOG(ERROR) << "Robust ReadPixels fallback: " << size
               << " bytes needed, but only " << buf_size << " provided.";
    return;
  }
  Fn().glReadPixelsFn(x, y, width, height, format, type, pixels);
  if (length) {
    *length = client_memory ? static_cast<GLsizei>(size) : 0;
  }
  if (columns) {
    *columns = width;
  }
  if (rows) {
    *rows = height;
  }
}

void GL_BINDING_CALL TexImage2DRobust(GLenum target,
                                      GLint level,
                                      GLint internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLint border,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei buf_size,
                                      const void* pixels) {
  if (!UploadFits(width, height, 1, format, type, buf_size, pixels)) {
    pixels = nullptr;
  }
  Fn().glTexImage2DFn(target, level, internalformat, width, height, border,
                      format, type, pixels);
}

void GL_BINDING_CALL TexImage3DRobust(GLenum target,
                                      GLint level,
                                      GLint internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLint border,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei buf_size,
                                      const void* pixels) {
  if (!Fn().glTexImage3DFn) {
    return;
  }
  if (!UploadFits(width, height, depth, format, type, buf_size, pixels)) {
    pixels = nullptr;
  }
  Fn().glTexImage3DFn(target, level, internalformat, width, height, depth,
                      border, format, type, pixels);
}

void GL_BINDING_CALL TexSubImage2DRobust(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei buf_size,
                                         const void* pixels) {
  if (!UploadFits(width, height, 1, format, type, buf_size, pixels)) {
    return;
  }
  Fn().glTexSubImage2DFn(target, level, xoffset, yoffset, width, height, format,
                         type, pixels);
}

void GL_BINDING_CALL TexSubImage3DRobust(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei buf_size,
                                         const void* pixels) {
  if (!Fn().glTexSubImage3DFn ||
      !UploadFits(width, height, depth, format, type, buf_size, pixels)) {
    return;
  }
  Fn().glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                         height, depth, format, type, pixels);
}

void GL_BINDING_CALL CompressedTexImage2DRobust(GLenum target,
                                                GLint level,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border,
                                                GLsizei image_size,
                                                GLsizei data_size,
                                                const void* data) {
  if (data && image_size > data_size) {
    return;
  }
  Fn().glCompressedTexImage2DFn(target, level, internalformat, width, height,
                                border, image_size, data);
}

void GL_BINDING_CALL CompressedTexImage3DRobust(GLenum target,
                                                GLint level,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLint border,
                                                GLsizei image_size,
                                                GLsizei data_size,
                                                const void* data) {
  if (!Fn().glCompressedTexImage3DFn || (data && image_size > data_size)) {
    return;
  }
  Fn().glCompressedTexImage3DFn(target, level, internalformat, width, height,
                                depth, border, image_size, data);
}

void GL_BINDING_CALL CompressedTexSubImage2DRobust(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLenum format,
                                                   GLsizei image_size,
                                                   GLsizei data_size,
                                                   const void* data) {
  if (data && image_size > data_size) {
    return;
  }
  Fn().glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                   height, format, image_size, data);
}

void GL_BINDING_CALL CompressedTexSubImage3DRobust(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLint zoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   GLenum format,
                                                   GLsizei image_size,
                                                   GLsizei data_size,
                                                   const void* data) {
  if (!Fn().glCompressedTexSubImage3DFn || (data && image_size > data_size)) {
    return;
  }
  Fn().glCompressedTexSubImage3DFn(target, level, xoffset, yoffset, zoffset,
                                   width, height, depth, format, image_size,
                                   data);
}

// ---- Parameter setters. |buf_size| is the number of values provided.

void GL_BINDING_CALL TexParameterfvRobust(GLenum target,
                                          GLenum pname,
                                          GLsizei buf_size,
                                          const GLfloat* params) {
  if (buf_size < TexParameterValueCount(pname)) {
    return;
  }
  Fn().glTexParameterfvFn(target, pname, params);
}

void GL_BINDING_CALL TexParameterivRobust(GLenum target,
                                          GLenum pname,
                                          GLsizei buf_size,
                                          const GLint* params) {
  if (buf_size < TexParameterValueCount(pname)) {
    return;
  }
  Fn().glTexParameterivFn(target, pname, params);
}

void GL_BINDING_CALL SamplerParameterfvRobust(GLuint sampler,
                                              GLenum pname,
                                              GLsizei buf_size,
                                              const GLfloat* params) {
  if (!Fn().glSamplerParameterfvFn ||
      buf_size < TexParameterValueCount(pname)) {
    return;
  }
  Fn().glSamplerParameterfvFn(sampler, pname, params);
}

void GL_BINDING_CALL SamplerParameterivRobust(GLuint sampler,
                                              GLenum pname,
                                              GLsizei buf_size,
                                              const GLint* params) {
  if (!Fn().glSamplerParameterivFn ||
      buf_size < TexParameterValueCount(pname)) {
    return;
  }
  Fn().glSamplerParameterivFn(sampler, pname, params);
}

#define LOOKED_UP_PARAMETER_SETTER_FALLBACK(Name, ObjectType, Type, Symbol)   \
  using Name##Proc = void(GL_BINDING_CALL*)(ObjectType, GLenum, const Type*); \
  void GL_BINDING_CALL Name##Robust(ObjectType object, GLenum pname,          \
                                    GLsizei buf_size, const Type* params) {   \
    static const Name##Proc fn = LookUp<Name##Proc>(Symbol);                  \
    if (!fn || buf_size < TexParameterValueCount(pname)) {                    \
      return;                                                                 \
    }                                                                         \
    fn(object, pname, params);                                                \
  }

LOOKED_UP_PARAMETER_SETTER_FALLBACK(TexParameterIiv,
                                    GLenum,
                                    GLint,
                                    "glTexParameterIiv")
LOOKED_UP_PARAMETER_SETTER_FALLBACK(TexParameterIuiv,
                                    GLenum,
                                    GLuint,
                                    "glTexParameterIuiv")
LOOKED_UP_PARAMETER_SETTER_FALLBACK(SamplerParameterIiv,
                                    GLuint,
                                    GLint,
                                    "glSamplerParameterIiv")
LOOKED_UP_PARAMETER_SETTER_FALLBACK(SamplerParameterIuiv,
                                    GLuint,
                                    GLuint,
                                    "glSamplerParameterIuiv")

#undef LOOKED_UP_PARAMETER_SETTER_FALLBACK

}  // namespace

void InstallRobustANGLEFallbacksIfNeeded(gl::DriverGL* driver) {
  if (!driver) {
    return;
  }
  gl::ProcsGL& fn = driver->fn;
  int installed = 0;
#define INSTALL(Name, Fallback)        \
  if (!fn.Name##RobustANGLEFn) {       \
    fn.Name##RobustANGLEFn = Fallback; \
    ++installed;                       \
  }

  INSTALL(glCompressedTexImage2D, &CompressedTexImage2DRobust)
  INSTALL(glCompressedTexImage3D, &CompressedTexImage3DRobust)
  INSTALL(glCompressedTexSubImage2D, &CompressedTexSubImage2DRobust)
  INSTALL(glCompressedTexSubImage3D, &CompressedTexSubImage3DRobust)
  INSTALL(glGetActiveUniformBlockiv, &GetActiveUniformBlockivRobust)
  INSTALL(glGetBooleani_v, &GetBooleani_vRobust)
  INSTALL(glGetBooleanv, &GetBooleanvRobust)
  INSTALL(glGetBufferParameteri64v, &GetBufferParameteri64vRobust)
  INSTALL(glGetBufferParameteriv, &GetBufferParameterivRobust)
  INSTALL(glGetBufferPointerv, &GetBufferPointervRobust)
  INSTALL(glGetFloatv, &GetFloatvRobust)
  INSTALL(glGetFramebufferAttachmentParameteriv,
          &GetFramebufferAttachmentParameterivRobust)
  INSTALL(glGetFramebufferParameteriv, &GetFramebufferParameterivRobust)
  INSTALL(glGetInteger64i_v, &GetInteger64i_vRobust)
  INSTALL(glGetInteger64v, &GetInteger64vRobust)
  INSTALL(glGetIntegeri_v, &GetIntegeri_vRobust)
  INSTALL(glGetIntegerv, &GetIntegervRobust)
  INSTALL(glGetInternalformativ, &GetInternalformativRobust)
  INSTALL(glGetMultisamplefv, &GetMultisamplefvRobust)
  INSTALL(glGetnUniformfv, &GetUniformfvRobust)
  INSTALL(glGetnUniformiv, &GetUniformivRobust)
  INSTALL(glGetnUniformuiv, &GetUniformuivRobust)
  INSTALL(glGetPointervRobustANGLE, &GetPointervRobust)
  INSTALL(glGetProgramInterfaceiv, &GetProgramInterfaceivRobust)
  INSTALL(glGetProgramiv, &GetProgramivRobust)
  INSTALL(glGetQueryiv, &GetQueryivRobust)
  INSTALL(glGetQueryObjecti64v, &GetQueryObjecti64vRobust)
  INSTALL(glGetQueryObjectiv, &GetQueryObjectivRobust)
  INSTALL(glGetQueryObjectui64v, &GetQueryObjectui64vRobust)
  INSTALL(glGetQueryObjectuiv, &GetQueryObjectuivRobust)
  INSTALL(glGetRenderbufferParameteriv, &GetRenderbufferParameterivRobust)
  INSTALL(glGetSamplerParameterfv, &GetSamplerParameterfvRobust)
  INSTALL(glGetSamplerParameterIiv, &GetSamplerParameterIivRobust)
  INSTALL(glGetSamplerParameterIuiv, &GetSamplerParameterIuivRobust)
  INSTALL(glGetSamplerParameteriv, &GetSamplerParameterivRobust)
  INSTALL(glGetShaderiv, &GetShaderivRobust)
  INSTALL(glGetTexLevelParameterfv, &GetTexLevelParameterfvRobust)
  INSTALL(glGetTexLevelParameteriv, &GetTexLevelParameterivRobust)
  INSTALL(glGetTexParameterfv, &GetTexParameterfvRobust)
  INSTALL(glGetTexParameterIiv, &GetTexParameterIivRobust)
  INSTALL(glGetTexParameterIuiv, &GetTexParameterIuivRobust)
  INSTALL(glGetTexParameteriv, &GetTexParameterivRobust)
  INSTALL(glGetUniformfv, &GetUniformfvRobust)
  INSTALL(glGetUniformiv, &GetUniformivRobust)
  INSTALL(glGetUniformuiv, &GetUniformuivRobust)
  INSTALL(glGetVertexAttribfv, &GetVertexAttribfvRobust)
  INSTALL(glGetVertexAttribIiv, &GetVertexAttribIivRobust)
  INSTALL(glGetVertexAttribIuiv, &GetVertexAttribIuivRobust)
  INSTALL(glGetVertexAttribiv, &GetVertexAttribivRobust)
  INSTALL(glGetVertexAttribPointerv, &GetVertexAttribPointervRobust)
  INSTALL(glReadnPixels, &ReadPixelsRobust)
  INSTALL(glReadPixels, &ReadPixelsRobust)
  INSTALL(glSamplerParameterfv, &SamplerParameterfvRobust)
  INSTALL(glSamplerParameterIiv, &SamplerParameterIivRobust)
  INSTALL(glSamplerParameterIuiv, &SamplerParameterIuivRobust)
  INSTALL(glSamplerParameteriv, &SamplerParameterivRobust)
  INSTALL(glTexImage2D, &TexImage2DRobust)
  INSTALL(glTexImage3D, &TexImage3DRobust)
  INSTALL(glTexParameterfv, &TexParameterfvRobust)
  INSTALL(glTexParameterIiv, &TexParameterIivRobust)
  INSTALL(glTexParameterIuiv, &TexParameterIuivRobust)
  INSTALL(glTexParameteriv, &TexParameterivRobust)
  INSTALL(glTexSubImage2D, &TexSubImage2DRobust)
  INSTALL(glTexSubImage3D, &TexSubImage3DRobust)

#undef INSTALL
  if (installed) {
    LOG(INFO) << "Installed " << installed
              << " GL_ANGLE_robust_client_memory fallbacks.";
  }
}

}  // namespace gles2
}  // namespace gpu
