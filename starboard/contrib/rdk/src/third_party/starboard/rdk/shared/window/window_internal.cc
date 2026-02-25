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

using namespace third_party::starboard::rdk::shared;

SbWindowPrivate::SbWindowPrivate(const SbWindowOptions* /* options */) { }

SbWindowPrivate::~SbWindowPrivate() = default;

void* SbWindowPrivate::Native() const {
  return reinterpret_cast<void*>(Application::Get()->GetNativeWindow());
}

int SbWindowPrivate::Width() const {
  return Application::Get()->GetWindowWidth();
}

int SbWindowPrivate::Height() const {
  return Application::Get()->GetWindowHeight();
}

float SbWindowPrivate::VideoPixelRatio() const {
  auto resolution_info = DisplayInfo::GetResolution();
  int window_height = Application::Get()->GetWindowHeight();
  float ratio = resolution_info.Height / static_cast<float>(window_height);
  float max_ratio = ( window_height < 1080 )
    ? 1.5f : ( resolution_info.Height <= 2160 ? 2.f : 4.f );
  return std::min(ratio, max_ratio);
}

float SbWindowPrivate::DiagonalSizeInInches() const {
  return DisplayInfo::GetDiagonalSizeInInches();
}
