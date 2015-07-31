// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_RENDERER_H_
#define MEDIA_BASE_VIDEO_RENDERER_H_

#include <list>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace gfx {
class Size;
}

namespace media {

class DemuxerStream;
class VideoDecoder;

class MEDIA_EXPORT VideoRenderer
    : public base::RefCountedThreadSafe<VideoRenderer> {
 public:
  typedef std::list<scoped_refptr<VideoDecoder> > VideoDecoderList;

  // Used to update the pipeline's clock time. The parameter is the time that
  // the clock should not exceed.
  typedef base::Callback<void(base::TimeDelta)> TimeCB;

  // Executed when the natural size of the video has changed.
  typedef base::Callback<void(const gfx::Size& size)> NaturalSizeChangedCB;

  // Used to query the current time or duration of the media.
  typedef base::Callback<base::TimeDelta()> TimeDeltaCB;

  // Initialize a VideoRenderer with the given DemuxerStream and
  // VideoDecoderList, executing |init_cb| callback upon completion.
  //
  // |statistics_cb| is executed periodically with video rendering stats, such
  // as dropped frames.
  //
  // |time_cb| is executed whenever time has advanced by way of video rendering.
  //
  // |size_changed_cb| is executed whenever the dimensions of the video has
  // changed.
  //
  // |ended_cb| is executed when video rendering has reached the end of stream.
  //
  // |error_cb| is executed if an error was encountered.
  //
  // |get_time_cb| is used to query the current media playback time.
  //
  // |get_duration_cb| is used to query the media duration.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const VideoDecoderList& decoders,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& time_cb,
                          const NaturalSizeChangedCB& size_changed_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const TimeDeltaCB& get_time_cb,
                          const TimeDeltaCB& get_duration_cb) = 0;

  // Start audio decoding and rendering at the current playback rate, executing
  // |callback| when playback is underway.
  virtual void Play(const base::Closure& callback) = 0;

  // Temporarily suspend decoding and rendering video, executing |callback| when
  // playback has been suspended.
  virtual void Pause(const base::Closure& callback) = 0;

  // Discard any video data, executing |callback| when completed.
  virtual void Flush(const base::Closure& callback) = 0;

  // Start prerolling video data for samples starting at |time|, executing
  // |callback| when completed.
  //
  // Only valid to call after a successful Initialize() or Flush().
  virtual void Preroll(base::TimeDelta time,
                       const PipelineStatusCB& callback) = 0;

  // Stop all operations in preparation for being deleted, executing |callback|
  // when complete.
  virtual void Stop(const base::Closure& callback) = 0;

  // Updates the current playback rate.
  virtual void SetPlaybackRate(float playback_rate) = 0;

 protected:
  friend class base::RefCountedThreadSafe<VideoRenderer>;

  VideoRenderer();
  virtual ~VideoRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoRenderer);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_RENDERER_H_
