// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

// Includes all headers in a C context to make sure they compile as C files.

#include "starboard/image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const SbDecodeTargetFormat kDecodeTargetFormats[] = {
  kSbDecodeTargetFormat1PlaneRGBA,
  kSbDecodeTargetFormat1PlaneBGRA,
  kSbDecodeTargetFormat2PlaneYUVNV12,
  kSbDecodeTargetFormat3PlaneYUVI420,
  kSbDecodeTargetFormat3Plane10BitYUVI420,
  kSbDecodeTargetFormat1PlaneUYVY,
  kSbDecodeTargetFormatInvalid,
};

const char* kMimeTypes[] = {
  "image/jpeg",
  "image/png",
  "image/gif",
  "application/json",
  "image/webp",
  "invalid",
};

// Verify SbImageIsDecodeSupported() can be called with any parameter values.
TEST(ImageTest, IsDecodeSupported) {
  for (size_t format = 0;
       format < sizeof(kDecodeTargetFormats) / sizeof(kDecodeTargetFormats[0]);
       ++format) {
    for (size_t mime_type = 0;
         mime_type < sizeof(kMimeTypes) / sizeof(kMimeTypes[0]); ++mime_type) {
      SbImageIsDecodeSupported(kMimeTypes[mime_type],
                               kDecodeTargetFormats[format]);
    }
  }
}

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
