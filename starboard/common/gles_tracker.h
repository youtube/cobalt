// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_GLES_TRACKER_H_
#define STARBOARD_COMMON_GLES_TRACKER_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <map>
#include <tuple>
#include "starboard/common/log.h"
#include "starboard/gles.h"

namespace starboard {
namespace common {
namespace gles_tracker {

#define GL_MEM_TRACE(x) trace_##x

enum Objects { Buffers = 0, Textures = 1, Renderbuffers = 2 };

inline const char* type2str(Objects type) {
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
  return "";
}

using MemObjects = std::map<GLuint, size_t>;
struct TrackedMemObject {
  GLuint active = 0;
  MemObjects objects;
  size_t total_allocation = 0;
};

inline TrackedMemObject* getObjects() {
  static TrackedMemObject objects[3] = {
      TrackedMemObject(),
      TrackedMemObject(),
      TrackedMemObject(),
  };
  return objects;
}

inline void genObjects(Objects type, GLsizei n, GLuint* objects) {
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
inline void deleteObjects(Objects type, GLsizei n, const GLuint* objects) {
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
inline void bindObject(Objects type, GLuint object) {
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
inline void reportAllocation(Objects type, size_t estimated_allocation) {
  TrackedMemObject& tracked_object = getObjects()[type];
  MemObjects& map = tracked_object.objects;
  auto existing = map.find(tracked_object.active);
  SB_CHECK(existing != map.end());
  auto previous = tracked_object.total_allocation;
  // First subtract the previous allocation
  tracked_object.total_allocation -= existing->second;
  // update and add
  existing->second = estimated_allocation;
  tracked_object.total_allocation += existing->second;
  auto mb_alloc = (tracked_object.total_allocation / (1024 * 1024));
  if (mb_alloc > 0 && previous != tracked_object.total_allocation) {
    SB_LOG(ERROR) << "GLTRACE: Alloc for type:" << type2str(type)
                  << " size: " << estimated_allocation
                  << " total:" << tracked_object.total_allocation << " (MB:"
                  << (tracked_object.total_allocation / (1024 * 1024)) << ")";
  }
}

// Buffers
inline void GL_MEM_TRACE(glGenBuffers)(GLsizei n, GLuint* buffers) {
  glGenBuffers(n, buffers);
  genObjects(Buffers, n, buffers);
}
inline void GL_MEM_TRACE(glDeleteBuffers)(GLsizei n, const GLuint* buffers) {
  glDeleteBuffers(n, buffers);
  deleteObjects(Buffers, n, buffers);
}
inline void GL_MEM_TRACE(glBindBuffer)(GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
  bindObject(Buffers, buffer);
}
inline void GL_MEM_TRACE(glBufferData)(GLenum target,
                                       GLsizeiptr size,
                                       const void* data,
                                       GLenum usage) {
  glBufferData(target, size, data, usage);
  reportAllocation(Buffers, size);
}

// Textures
inline void GL_MEM_TRACE(glGenTextures)(GLsizei n, GLuint* textures) {
  glGenTextures(n, textures);
  genObjects(Textures, n, textures);
}
inline void GL_MEM_TRACE(glDeleteTextures)(GLsizei n, const GLuint* textures) {
  glDeleteTextures(n, textures);
  deleteObjects(Textures, n, textures);
}
inline void GL_MEM_TRACE(glBindTexture)(GLenum target, GLuint texture) {
  glBindTexture(target, texture);
  bindObject(Textures, texture);
}

inline size_t estimate_bytes_per_pixel(GLenum format) {
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

inline void GL_MEM_TRACE(glTexImage2D)(GLenum target,
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
inline void GL_MEM_TRACE(glTexSubImage2D)(GLenum target,
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
inline void GL_MEM_TRACE(glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers) {
  glGenRenderbuffers(n, renderbuffers);
  genObjects(Renderbuffers, n, renderbuffers);
}
inline void GL_MEM_TRACE(glDeleteRenderbuffers)(GLsizei n,
                                                const GLuint* renderbuffers) {
  glDeleteRenderbuffers(n, renderbuffers);
  deleteObjects(Renderbuffers, n, renderbuffers);
}
inline void GL_MEM_TRACE(glBindRenderbuffer)(GLenum target,
                                             GLuint renderbuffer) {
  glBindRenderbuffer(target, renderbuffer);
  bindObject(Renderbuffers, renderbuffer);
}
inline void GL_MEM_TRACE(glRenderbufferStorage)(GLenum target,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height) {
  glRenderbufferStorage(target, internalformat, width, height);
  reportAllocation(Renderbuffers, width * height * 4);
}

// Disable all extensions
inline const GLubyte* patch_glGetString(GLenum name) {
  if (name == GL_EXTENSIONS) {
    static const unsigned char dummy[1] = {'\0'};
    return dummy;
  }
  return glGetString(name);
}

}  // namespace gles_tracker
}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_GLES_TRACKER_H_
