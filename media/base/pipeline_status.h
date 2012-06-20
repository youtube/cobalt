// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_STATUS_H_
#define MEDIA_BASE_PIPELINE_STATUS_H_

#include "base/callback.h"

#include <string>

namespace media {

// Status states for pipeline.  All codes except PIPELINE_OK indicate errors.
enum PipelineStatus {
  PIPELINE_OK,
  PIPELINE_ERROR_URL_NOT_FOUND,
  PIPELINE_ERROR_NETWORK,
  PIPELINE_ERROR_DECODE,
  PIPELINE_ERROR_DECRYPT,
  PIPELINE_ERROR_ABORT,
  PIPELINE_ERROR_INITIALIZATION_FAILED,
  PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
  PIPELINE_ERROR_COULD_NOT_RENDER,
  PIPELINE_ERROR_READ,
  PIPELINE_ERROR_OPERATION_PENDING,
  PIPELINE_ERROR_INVALID_STATE,
  // Demuxer related errors.
  DEMUXER_ERROR_COULD_NOT_OPEN,
  DEMUXER_ERROR_COULD_NOT_PARSE,
  DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
  // Decoder related errors.
  DECODER_ERROR_NOT_SUPPORTED,
  PIPELINE_STATUS_MAX,  // Must be greater than all other values logged.
};

typedef base::Callback<void(PipelineStatus)> PipelineStatusCB;

// Wrap & return a callback around |cb| which reports its argument to UMA under
// the requested |name|.
PipelineStatusCB CreateUMAReportingPipelineCB(const std::string& name,
                                              const PipelineStatusCB& cb);

// TODO(scherkus): this should be moved alongside host interface definitions.
struct PipelineStatistics {
  PipelineStatistics()
      : audio_bytes_decoded(0),
        video_bytes_decoded(0),
        video_frames_decoded(0),
        video_frames_dropped(0) {
  }

  uint32 audio_bytes_decoded;  // Should be uint64?
  uint32 video_bytes_decoded;  // Should be uint64?
  uint32 video_frames_decoded;
  uint32 video_frames_dropped;
};

// Used for updating pipeline statistics.
typedef base::Callback<void(const PipelineStatistics&)> StatisticsCB;

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_STATUS_H_
