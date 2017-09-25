// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_VIDEO_TRANSFORM_H_
#define STARBOARD_SHARED_WIN32_VIDEO_TRANSFORM_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/shared/win32/media_transform.h"

namespace starboard {
namespace shared {
namespace win32 {

enum VideoFormat {
  kVideoFormat_YV12,
};

// Creating an H264 decoder should succeed unconditionally.
scoped_ptr<MediaTransform> CreateH264Transform(VideoFormat video_fmt);

// VP9 decoders require a width/height variable and may fail construction for
// certain configurations.
scoped_ptr<MediaTransform> TryCreateVP9Transform(VideoFormat video_fmt,
                                                 int width,
                                                 int height);

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_TRANSFORM_H_
