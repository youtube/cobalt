// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_SPATIAL_FORMAT_H_
#define MEDIA_BASE_VIDEO_SPATIAL_FORMAT_H_

#include <string>

#include "media/base/media_export.h"

namespace media {

// Represents the stereoscopic video layout.
//
// Note: While container specifications (such as WebM/Matroska's StereoMode
// values and MP4's st3d) support a wide variety of stereoscopic layouts, we
// only define and support a subset here:
// - kMono: Standard 2D flat video.
// - kSideBySideLeftFirst / kTopBottomLeftFirst: These are the standard and most
//   common formats used for modern VR and 3D video delivery.
//
// Obsolete, legacy hardware-specific layouts (such as checkerboard, row/column
// interleaved, and laced formats), rendering-time effects (such as anaglyph),
// and custom, unrecognized, or unsupported layouts are mapped to kMono.
enum class VideoStereoMode {
  kMono = 0,
  kSideBySideLeftFirst,
  kTopBottomLeftFirst,
  kMaxValue = kTopBottomLeftFirst,
};

// Represents the projection format of a video track.
enum class VideoProjectionType {
  kNone = 0,     // Standard flat video
  kEquirect180,  // 180-degree VR video
  kEquirect360,  // 360-degree VR video
  kMaxValue = kEquirect360,
};

// Represents the spatial format of a video track.
struct MEDIA_EXPORT VideoSpatialFormat {
  VideoProjectionType projection_type = VideoProjectionType::kNone;
  VideoStereoMode stereo_mode = VideoStereoMode::kMono;

  bool operator==(const VideoSpatialFormat& other) const = default;

  std::string ToString() const;
};


}  // namespace media

#endif  // MEDIA_BASE_VIDEO_SPATIAL_FORMAT_H_
