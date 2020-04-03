// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_PIPELINE_METADATA_H_
#define COBALT_MEDIA_BASE_PIPELINE_METADATA_H_

#include "base/time/time.h"
#include "cobalt/math/size.h"
#include "cobalt/media/base/video_rotation.h"

namespace cobalt {
namespace media {

// Metadata describing a pipeline once it has been initialized.
struct PipelineMetadata {
  PipelineMetadata()
      : has_audio(false), has_video(false), video_rotation(VIDEO_ROTATION_0) {}

  bool has_audio;
  bool has_video;
  math::Size natural_size;
  VideoRotation video_rotation;
  base::Time timeline_offset;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_PIPELINE_METADATA_H_
