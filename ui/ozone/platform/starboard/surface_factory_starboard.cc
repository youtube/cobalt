// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "ui/ozone/platform/starboard/surface_factory_starboard.h"

namespace ui {

SurfaceFactoryStarboard::SurfaceFactoryStarboard() = default;

SurfaceFactoryStarboard::~SurfaceFactoryStarboard() = default;

std::vector<gl::GLImplementationParts>
SurfaceFactoryStarboard::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
  };
}

GLOzone* SurfaceFactoryStarboard::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
      return &egl_implementation_;
    default:
      return nullptr;
  }
}
}  // namespace ui
