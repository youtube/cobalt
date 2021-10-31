// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/blitter_helpers.h"

#include "starboard/blitter.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {

std::vector<SbBlitterPixelDataFormat> GetAllSupportedPixelFormatsForPixelData(
    SbBlitterDevice device) {
  std::vector<SbBlitterPixelDataFormat> ret;

  for (int i = 0; i < kSbBlitterNumPixelDataFormats; ++i) {
    if (SbBlitterIsPixelFormatSupportedByPixelData(
            device, static_cast<SbBlitterPixelDataFormat>(i))) {
      ret.push_back(static_cast<SbBlitterPixelDataFormat>(i));
    }
  }

  return ret;
}

std::vector<SbBlitterPixelDataFormat> GetAllUnsupportedPixelFormatsForPixelData(
    SbBlitterDevice device) {
  std::vector<SbBlitterPixelDataFormat> ret;

  for (int i = 0; i < kSbBlitterNumPixelDataFormats; ++i) {
    if (!SbBlitterIsPixelFormatSupportedByPixelData(
            device, static_cast<SbBlitterPixelDataFormat>(i))) {
      ret.push_back(static_cast<SbBlitterPixelDataFormat>(i));
    }
  }

  return ret;
}

SurfaceFormats GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(
    SbBlitterDevice device) {
  SurfaceFormats ret;

  for (int i = 0; i < kSbBlitterNumSurfaceFormats; ++i) {
    if (SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(
            device, static_cast<SbBlitterSurfaceFormat>(i))) {
      ret.push_back(static_cast<SbBlitterSurfaceFormat>(i));
    }
  }

  return ret;
}

SurfaceFormats GetAllUnsupportedSurfaceFormatsForRenderTargetSurfaces(
    SbBlitterDevice device) {
  SurfaceFormats ret;

  for (int i = 0; i < kSbBlitterNumSurfaceFormats; ++i) {
    if (!SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(
            device, static_cast<SbBlitterSurfaceFormat>(i))) {
      ret.push_back(static_cast<SbBlitterSurfaceFormat>(i));
    }
  }

  return ret;
}

SbBlitterSurface CreateArbitraryRenderTargetSurface(SbBlitterDevice device,
                                                    int width,
                                                    int height) {
  SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
      device, width, height,
      GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(device)[0]);
  EXPECT_TRUE(SbBlitterIsSurfaceValid(surface));
  return surface;
}

ContextTestEnvironment::ContextTestEnvironment(int width, int height) {
  device_ = SbBlitterCreateDefaultDevice();
  EXPECT_TRUE(SbBlitterIsDeviceValid(device_));

  surface_ = CreateArbitraryRenderTargetSurface(device_, width, height);

  render_target_ = SbBlitterGetRenderTargetFromSurface(surface_);
  EXPECT_TRUE(SbBlitterIsRenderTargetValid(render_target_));

  context_ = SbBlitterCreateContext(device_);
  EXPECT_TRUE(SbBlitterIsContextValid(context_));
}

ContextTestEnvironment::~ContextTestEnvironment() {
  EXPECT_TRUE(SbBlitterDestroyContext(context_));
  EXPECT_TRUE(SbBlitterDestroySurface(surface_));
  EXPECT_TRUE(SbBlitterDestroyDevice(device_));
}

}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
