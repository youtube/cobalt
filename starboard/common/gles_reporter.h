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

#ifndef STARBOARD_COMMON_GLES_REPORTER_H_
#define STARBOARD_COMMON_GLES_REPORTER_H_

#include <map>
#include <string>
#include <vector>

// This module contains GLES memory tracking infra
// parts that can be used without dependency on GLES headers

#if !defined(COBALT_BUILD_TYPE_GOLD)

namespace starboard {
namespace common {
namespace gles_tracker {

enum Objects { Buffers = 0, Textures = 1, Renderbuffers = 2 };

struct AllocationInfo {
  size_t size;
  std::vector<std::string> creation_contexts;
};

// uint32_t = GLuint
using MemObjects = std::map<uint32_t, AllocationInfo>;
struct TrackedMemObject {
  uint32_t active = 0;
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

inline void GetTotalGpuMem(size_t* buffers,
                           size_t* textures,
                           size_t* render_buffers) {
  TrackedMemObject* tracked_objects = getObjects();
  *buffers = tracked_objects[Buffers].total_allocation;
  *textures = tracked_objects[Textures].total_allocation;
  *render_buffers = tracked_objects[Renderbuffers].total_allocation;
}

inline void DumpTotalGpuMem() {
  size_t buffers, textures, render_buffers;
  GetTotalGpuMem(&buffers, &textures, &render_buffers);
  SB_LOG(INFO) << "GLTRACE: Total GPU Memory Usage:";
  SB_LOG(INFO) << "GLTRACE:   Buffers: " << buffers << " bytes ("
               << (buffers / (1024 * 1024)) << " MB)";
  SB_LOG(INFO) << "GLTRACE:   Textures: " << textures << " bytes ("
               << (textures / (1024 * 1024)) << " MB)";
  SB_LOG(INFO) << "GLTRACE:   Renderbuffers: " << render_buffers << " bytes ("
               << (render_buffers / (1024 * 1024)) << " MB)";
}

}  // namespace gles_tracker
}  // namespace common
}  // namespace starboard

#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

#endif  // STARBOARD_COMMON_GLES_REPORTER_H_
