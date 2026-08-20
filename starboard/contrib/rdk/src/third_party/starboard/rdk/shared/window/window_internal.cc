//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "third_party/starboard/rdk/shared/window/window_internal.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/platform/platform_interface.h"

using namespace third_party::starboard::rdk::shared;
using ::starboard::ApplicationRdk;

SbWindowPrivate::SbWindowPrivate(const SbWindowOptions* /* options */) { }

SbWindowPrivate::~SbWindowPrivate() { }

void* SbWindowPrivate::Native() const {
  return reinterpret_cast<void*>(ApplicationRdk::Get()->GetNativeWindow());
}

int SbWindowPrivate::Width() const {
  return ApplicationRdk::Get()->GetWindowWidth();
}

int SbWindowPrivate::Height() const {
  return ApplicationRdk::Get()->GetWindowHeight();
}

float SbWindowPrivate::VideoPixelRatio() const {
  auto video_resolution = platform::device().video_resolution().value_or(platform::Resolution{});
  int window_height = ApplicationRdk::Get()->GetWindowHeight();
  float ratio = video_resolution.height / static_cast<float>(window_height);
  float max_ratio = ( window_height < 1080 )
    ? 1.5f : ( video_resolution.height <= 2160 ? 2.f : 4.f );
  return std::min(ratio, max_ratio);
}

float SbWindowPrivate::DiagonalSizeInInches() const {
  return platform::device().diagonal_size_in_inches().value_or(.0f);
}
