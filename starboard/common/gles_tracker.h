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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/gles.h"
#include "starboard/shared/environment.h"

#include "starboard/common/gles_reporter.h"

// This module contains GLES memory tracking infra
// parts that need GLES headers

#if defined(COBALT_BUILD_TYPE_GOLD)
#define GL_MEM_TRACE_I(x) x
#else
#define GL_MEM_TRACE_I(x) starboard::common::gles_tracker::trace_##x

namespace starboard {
namespace common {
namespace gles_tracker {

#define GL_MEM_TRACE(x) trace_##x

inline const char* type2str(ObjectType type) {
  switch (type) {
    case ObjectType::kBuffers:
      return "buffers";
    case ObjectType::kTextures:
      return "textures";
    case ObjectType::kRenderbuffers:
      return "renderbuffers";
    default:
      SB_NOTREACHED();
  }
  return "";
}

inline bool verbose_reporting() {
  static const bool is_verbose = []() {
    std::string env_val =
        starboard::GetEnvironment("COBALT_GLES_VERBOSE_ALLOCATION_REPORTING");
    return !env_val.empty();
  }();
  return is_verbose;
}

inline void genObjects(ObjectType type, GLsizei n, GLuint* objects) {
  TrackedMemObject& tracked_object = getObjects()[static_cast<size_t>(type)];
  MemObjects& map = tracked_object.objects;
  for (GLsizei i = 0; i < n; i++) {
    GLuint object_id = objects[i];
    auto existing = map.find(object_id);
    SB_CHECK(existing == map.end());
    map.insert({object_id, AllocationInfo()});
    if (verbose_reporting()) {
      SB_LOG(INFO) << "GLTRACE: added type:" << type2str(type)
                   << " object:" << object_id << " p:" << &tracked_object;
    }
  }
}
inline void deleteObjects(ObjectType type, GLsizei n, const GLuint* objects) {
  TrackedMemObject& tracked_object = getObjects()[static_cast<size_t>(type)];
  MemObjects& map = tracked_object.objects;
  for (GLsizei i = 0; i < n; i++) {
    GLuint object_id = objects[i];
    auto existing = map.find(object_id);
    SB_CHECK(existing != map.end());
    if (existing->second.size != 0) {
      SB_LOG(WARNING)
          << "GLTRACE: Deleting object with non-zero tracked size. type:"
          << type2str(type) << " size: " << existing->second.size
          << " total:" << tracked_object.total_allocation
          << " (MB:" << (tracked_object.total_allocation / (1024 * 1024))
          << ")";
    }
    tracked_object.total_allocation -= existing->second.size;
    map.erase(existing);
  }
}
inline void bindObject(ObjectType type, GLuint object) {
  TrackedMemObject& tracked_object = getObjects()[static_cast<size_t>(type)];
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
inline void reportAllocation(ObjectType type, size_t estimated_allocation) {
  TrackedMemObject& tracked_object = getObjects()[static_cast<size_t>(type)];
  MemObjects& map = tracked_object.objects;
  auto existing = map.find(tracked_object.active);
  SB_CHECK(existing != map.end());
  auto previous = tracked_object.total_allocation;
  // First subtract the previous allocation
  tracked_object.total_allocation -= existing->second.size;
  // update and add
  existing->second.size = estimated_allocation;
  tracked_object.total_allocation += existing->second.size;
  auto mb_alloc = (tracked_object.total_allocation / (1024 * 1024));
  if (mb_alloc > 0 && previous != tracked_object.total_allocation) {
    if (verbose_reporting()) {
      SB_LOG(INFO) << "GLTRACE: Alloc for type:" << type2str(type)
                   << " size: " << estimated_allocation
                   << " total:" << tracked_object.total_allocation << " (MB:"
                   << (tracked_object.total_allocation / (1024 * 1024)) << ")";
    }
  }
}

// Buffers
inline void GL_MEM_TRACE(glGenBuffers)(GLsizei n, GLuint* buffers) {
  glGenBuffers(n, buffers);
  genObjects(ObjectType::kBuffers, n, buffers);
}
inline void GL_MEM_TRACE(glDeleteBuffers)(GLsizei n, const GLuint* buffers) {
  glDeleteBuffers(n, buffers);
  deleteObjects(ObjectType::kBuffers, n, buffers);
}
inline void GL_MEM_TRACE(glBindBuffer)(GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
  bindObject(ObjectType::kBuffers, buffer);
}
inline void GL_MEM_TRACE(glBufferData)(GLenum target,
                                       GLsizeiptr size,
                                       const void* data,
                                       GLenum usage) {
  glBufferData(target, size, data, usage);
  reportAllocation(ObjectType::kBuffers, size);
}

// Textures
inline void GL_MEM_TRACE(glGenTextures)(GLsizei n, GLuint* textures) {
  glGenTextures(n, textures);
  genObjects(ObjectType::kTextures, n, textures);
}
inline void GL_MEM_TRACE(glDeleteTextures)(GLsizei n, const GLuint* textures) {
  glDeleteTextures(n, textures);
  deleteObjects(ObjectType::kTextures, n, textures);
}
inline void GL_MEM_TRACE(glBindTexture)(GLenum target, GLuint texture) {
  glBindTexture(target, texture);
  bindObject(ObjectType::kTextures, texture);
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
      SB_LOG(WARNING) << "GLTRACE: Unhandled texture format " << format
                      << " in estimate_bytes_per_pixel. Over-estimating size.";
      return 8;  // overcount, to err on high side
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
  reportAllocation(ObjectType::kTextures,
                   width * height * estimate_bytes_per_pixel(format));
}

// Renderbuffers
inline void GL_MEM_TRACE(glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers) {
  glGenRenderbuffers(n, renderbuffers);
  genObjects(ObjectType::kRenderbuffers, n, renderbuffers);
}
inline void GL_MEM_TRACE(glDeleteRenderbuffers)(GLsizei n,
                                                const GLuint* renderbuffers) {
  glDeleteRenderbuffers(n, renderbuffers);
  deleteObjects(ObjectType::kRenderbuffers, n, renderbuffers);
}
inline void GL_MEM_TRACE(glBindRenderbuffer)(GLenum target,
                                             GLuint renderbuffer) {
  glBindRenderbuffer(target, renderbuffer);
  bindObject(ObjectType::kRenderbuffers, renderbuffer);
}
inline void GL_MEM_TRACE(glRenderbufferStorage)(GLenum target,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height) {
  glRenderbufferStorage(target, internalformat, width, height);
  reportAllocation(ObjectType::kRenderbuffers, width * height * 4);
}

// Disable all extensions
inline const GLubyte* patch_glGetString(GLenum name) {
  if (name == GL_EXTENSIONS) {
    static const bool kDisableExtensions = []() {
      std::string env_val =
          starboard::GetEnvironment("COBALT_EGL_DISABLE_EXTENSIONS");
      return !env_val.empty();
    }();
    if (kDisableExtensions) {
      static const unsigned char dummy[1] = {'\0'};
      return dummy;
    }
  }
  return glGetString(name);
}

inline const GLubyte* GL_MEM_TRACE(glGetString)(GLenum name) {
  return patch_glGetString(name);
}

}  // namespace gles_tracker
}  // namespace common
}  // namespace starboard

#endif

#endif  // STARBOARD_COMMON_GLES_TRACKER_H_
