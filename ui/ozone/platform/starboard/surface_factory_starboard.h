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

#ifndef UI_OZONE_PLATFORM_STARBOARD_SURFACE_FACTORY_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_SURFACE_FACTORY_STARBOARD_H_

#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class SurfaceFactoryStarboard : public SurfaceFactoryOzone {
 public:
  SurfaceFactoryStarboard();

  SurfaceFactoryStarboard(const SurfaceFactoryStarboard&) = delete;
  SurfaceFactoryStarboard& operator=(const SurfaceFactoryStarboard&) = delete;

  ~SurfaceFactoryStarboard() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_SURFACE_FACTORY_STARBOARD_H_
