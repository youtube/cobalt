// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_OPENH264_OPENH264_LIBRARY_LOADER_H_
#define STARBOARD_SHARED_OPENH264_OPENH264_LIBRARY_LOADER_H_

#include "starboard/shared/internal_only.h"
#include "third_party/openh264/include/codec_api.h"
#include "third_party/openh264/include/codec_app_def.h"
#include "third_party/openh264/include/codec_def.h"

namespace starboard {
namespace shared {
namespace openh264 {

bool is_openh264_supported();

extern int (*WelsCreateDecoder)(ISVCDecoder** ppDecoder);

extern void (*WelsDestroyDecoder)(ISVCDecoder* pDecoder);

}  // namespace openh264
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_OPENH264_OPENH264_LIBRARY_LOADER_H_
