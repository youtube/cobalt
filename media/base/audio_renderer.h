// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_H_
#define MEDIA_BASE_AUDIO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "media/base/filters.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class AudioDecoder;

class MEDIA_EXPORT AudioRenderer : public Filter {
 public:
  // Used to update the pipeline's clock time. The first parameter is the
  // current time, and the second parameter is the time that the clock must not
  // exceed.
  typedef base::Callback<void(base::TimeDelta, base::TimeDelta)> TimeCB;

  // Initialize a AudioRenderer with the given AudioDecoder, executing the
  // |init_cb| upon completion. |underflow_cb| is called when the
  // renderer runs out of data to pass to the audio card during playback.
  // If the |underflow_cb| is called ResumeAfterUnderflow() must be called
  // to resume playback. Pause(), Seek(), or Stop() cancels the underflow
  // condition.
  virtual void Initialize(const scoped_refptr<AudioDecoder>& decoder,
                          const PipelineStatusCB& init_cb,
                          const base::Closure& underflow_cb,
                          const TimeCB& time_cb) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

  // Resumes playback after underflow occurs.
  // |buffer_more_audio| is set to true if you want to increase the size of the
  // decoded audio buffer.
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) = 0;

 protected:
  virtual ~AudioRenderer() {}
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_H_
