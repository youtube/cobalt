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

#ifndef STARBOARD_NPLB_BLITTER_HELPERS_H_
#define STARBOARD_NPLB_BLITTER_HELPERS_H_

#include <vector>

#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {

std::vector<SbBlitterPixelDataFormat> GetAllSupportedPixelFormatsForPixelData(
    SbBlitterDevice device);

std::vector<SbBlitterPixelDataFormat> GetAllUnsupportedPixelFormatsForPixelData(
    SbBlitterDevice device);

typedef std::vector<SbBlitterSurfaceFormat> SurfaceFormats;
SurfaceFormats GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(
    SbBlitterDevice device);

SurfaceFormats GetAllUnsupportedSurfaceFormatsForRenderTargetSurfaces(
    SbBlitterDevice device);

SbBlitterSurface CreateArbitraryRenderTargetSurface(SbBlitterDevice device,
                                                    int width,
                                                    int height);

// Constructing this helper class will automatically create all objects
// necessary for testing functions that require SbBlitterContext objects.
// It will create a SbBlitterDevice, SbBlitterRenderTarget and SbBlitterContext.
class ContextTestEnvironment {
 public:
  ContextTestEnvironment(int width, int height);
  ~ContextTestEnvironment();

  SbBlitterDevice device() { return device_; }
  SbBlitterRenderTarget render_target() { return render_target_; }
  SbBlitterSurface surface() { return surface_; }
  SbBlitterContext context() { return context_; }

 private:
  SbBlitterDevice device_;
  SbBlitterSurface surface_;
  SbBlitterRenderTarget render_target_;
  SbBlitterContext context_;
};

}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)

#endif  // STARBOARD_NPLB_BLITTER_HELPERS_H_
