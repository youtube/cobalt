// Copyright 2016 Samsung Electronics. All Rights Reserved.
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

#include "starboard/shared/wayland/window_internal.h"

namespace {
const int kWindowWidth = 1920;
const int kWindowHeight = 1080;
}

SbWindowPrivate::SbWindowPrivate(const SbWindowOptions* options,
                                 float pixel_ratio) {
  width = kWindowWidth;
  height = kWindowHeight;
  video_pixel_ratio = pixel_ratio;
  if (options && options->size.width > 0 && options->size.height > 0) {
    width = options->size.width;
    height = options->size.height;
  }
}
