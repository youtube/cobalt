// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECODE_METADATA_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECODE_METADATA_H_

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_aspect_ratio.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

struct MEDIA_GPU_EXPORT VideoToolboxDecodeMetadata {
  VideoToolboxDecodeMetadata();
  ~VideoToolboxDecodeMetadata();

  scoped_refptr<CodecPicture> picture;

  base::TimeDelta timestamp = kNoTimestamp;
  base::TimeDelta duration = kNoTimestamp;
  VideoAspectRatio aspect_ratio;
  gfx::ColorSpace color_space;
  absl::optional<gfx::HDRMetadata> hdr_metadata;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECODE_METADATA_H_
