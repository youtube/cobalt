// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_STATUS_H_
#define MEDIA_BASE_PIPELINE_STATUS_H_

#include "base/callback.h"

namespace media {

// Error definitions for pipeline.  All codes indicate an error except:
// PIPELINE_OK indicates the pipeline is running normally.
enum PipelineError {
  PIPELINE_OK,
  PIPELINE_ERROR_URL_NOT_FOUND,
  PIPELINE_ERROR_NETWORK,
  PIPELINE_ERROR_DECODE,
  PIPELINE_ERROR_ABORT,
  PIPELINE_ERROR_INITIALIZATION_FAILED,
  PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
  PIPELINE_ERROR_OUT_OF_MEMORY,
  PIPELINE_ERROR_COULD_NOT_RENDER,
  PIPELINE_ERROR_READ,
  PIPELINE_ERROR_AUDIO_HARDWARE,
  PIPELINE_ERROR_OPERATION_PENDING,
  PIPELINE_ERROR_INVALID_STATE,
  // Demuxer related errors.
  DEMUXER_ERROR_COULD_NOT_OPEN,
  DEMUXER_ERROR_COULD_NOT_PARSE,
  DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
  DEMUXER_ERROR_COULD_NOT_CREATE_THREAD,
  // DataSourceFactory errors.
  DATASOURCE_ERROR_URL_NOT_SUPPORTED,
};

typedef Callback1<media::PipelineError>::Type PipelineStatusCallback;

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_STATUS_H_
