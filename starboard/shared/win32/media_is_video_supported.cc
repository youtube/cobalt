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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include <sstream>

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/once.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/media_transform.h"
#include "starboard/shared/win32/video_transform.h"
#include "starboard/window.h"

// #define ENABLE_VP9_DECODER

using Microsoft::WRL::ComPtr;
using starboard::scoped_ptr;
using starboard::ScopedLock;
using starboard::shared::win32::CheckResult;
using starboard::shared::win32::kVideoFormat_YV12;
using starboard::shared::win32::MediaTransform;
using starboard::shared::win32::TryCreateVP9Transform;

namespace {

class VideoSupported {
 public:
  static VideoSupported* GetSingleton();
  bool IsVideoSupported(SbMediaVideoCodec video_codec,
                        int frame_width,
                        int frame_height,
                        int64_t bitrate,
                        int fps) {
    // Is resolution out of range?
    if (frame_width > max_width_ || frame_height > max_height_) {
      return false;
    }
    // Is bitrate in range?
    if (bitrate > SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND) {
      return false;
    }
    if (fps > 60) {
      return false;
    }
    if (video_codec == kSbMediaVideoCodecH264) {
      return true;
    }
    if ((video_codec == kSbMediaVideoCodecVp9) && AllowVp9Decoder()) {
      return IsVp9Supported(frame_width, frame_height);
    }
    return false;
  }

 private:
  static bool AllowVp9Decoder() {
#ifdef ENABLE_VP9_DECODER
    return true;
#else
    return false;
#endif
  }

  static bool DetectVp9Supported(int width, int height) {
    scoped_ptr<MediaTransform> vp9_decoder =
        TryCreateVP9Transform(kVideoFormat_YV12, width, height);
    return !!vp9_decoder.get();
  }

  bool IsVp9Supported(int width, int height) {
    // When width/height is zero then this is a special value to mean
    // IsVP9 available at all? Therefore detect the general case by
    // testing the specific case of 1024 x 768.
    if ((width == 0) && (height == 0)) {
      return DetectVp9Supported(1024, 768);
    }

    Key key = {width, height};
    ScopedLock lock(mutex_);
    auto it = vpn_size_cache_.find(key);
    if (it != vpn_size_cache_.end()) {
      return it->second;
    } else {
      const bool vp9_valid = DetectVp9Supported(width, height);
      vpn_size_cache_[key] = vp9_valid;
      return vp9_valid;
    }
  }

  VideoSupported() : max_width_(0), max_height_(0) {
    Construct();
  }

  void Construct() {
    SbWindowOptions sb_window_options;
    SbWindowSetDefaultOptions(&sb_window_options);
    max_width_ = sb_window_options.size.width;
    max_height_ = sb_window_options.size.height;
  }

  struct Key {
    int width = 0;
    int height = 0;
    bool operator<(const Key& other) const {
      if (width != other.width) {
        return width < other.width;
      }
      return height < other.height;
    }
  };

  starboard::Mutex mutex_;
  using Vp9SizeCache = std::map<Key, bool>;
  Vp9SizeCache vpn_size_cache_;
  int max_width_;
  int max_height_;
};

SB_ONCE_INITIALIZE_FUNCTION(VideoSupported, VideoSupported::GetSingleton);

}  // namespace.

SB_EXPORT bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                                       int frame_width,
                                       int frame_height,
                                       int64_t bitrate,
                                       int fps) {
  bool supported = VideoSupported::GetSingleton()->IsVideoSupported(
      video_codec, frame_width, frame_height, bitrate, fps);
  return supported;
}
