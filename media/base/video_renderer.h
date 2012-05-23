// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_RENDERER_H_
#define MEDIA_BASE_VIDEO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "media/base/filters.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class VideoDecoder;

class MEDIA_EXPORT VideoRenderer : public Filter {
 public:
  // Used to update the pipeline's clock time. The parameter is the time that
  // the clock should not exceed.
  typedef base::Callback<void(base::TimeDelta)> TimeCB;

  // Initialize a VideoRenderer with the given VideoDecoder, executing the
  // callback upon completion.
  virtual void Initialize(const scoped_refptr<VideoDecoder>& decoder,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& time_cb) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;

 protected:
  virtual ~VideoRenderer() {}
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_RENDERER_H_
