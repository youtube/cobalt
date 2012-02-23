// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_generator.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/demuxer_stream.h"

namespace media {

VideoFrameGenerator::VideoFrameGenerator(
    base::MessageLoopProxy* message_loop_proxy,
    const gfx::Size& size,
    const base::TimeDelta& frame_duration)
    : message_loop_proxy_(message_loop_proxy),
      natural_size_(size),
      stopped_(true),
      frame_duration_(frame_duration) {
}

VideoFrameGenerator::~VideoFrameGenerator() {}

void VideoFrameGenerator::Initialize(
    DemuxerStream* demuxer_stream,
    const PipelineStatusCB& filter_callback,
    const StatisticsCallback& stat_callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::InitializeOnDecoderThread,
                 this, make_scoped_refptr(demuxer_stream),
                 filter_callback, stat_callback));
}

void VideoFrameGenerator::Read(const ReadCB& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::ReadOnDecoderThread,
                 this, callback));
}

const gfx::Size& VideoFrameGenerator::natural_size() {
  return natural_size_;
}

void VideoFrameGenerator::Stop(const base::Closure& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::StopOnDecoderThread,
                 this, callback));
}

void VideoFrameGenerator::InitializeOnDecoderThread(
    DemuxerStream* demuxer_stream,
    const PipelineStatusCB& filter_callback,
    const StatisticsCallback& stat_callback) {
  DVLOG(1) << "InitializeOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  filter_callback.Run(PIPELINE_OK);
  stopped_ = false;
}

void VideoFrameGenerator::ReadOnDecoderThread(const ReadCB& callback) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  CHECK(!callback.is_null());
  if (stopped_)
    return;

  // Always allocate a new frame.
  //
  // TODO(scherkus): migrate this to proper buffer recycling.
  scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateFrame(VideoFrame::YV12,
                              natural_size_.width(),
                              natural_size_.height(),
                              current_time_,
                              frame_duration_);
  current_time_ += frame_duration_;

  // TODO(wjia): set pixel data to pre-defined patterns if it's desired to
  // verify frame content.

  callback.Run(video_frame);
}

void VideoFrameGenerator::StopOnDecoderThread(
    const base::Closure& callback) {
  DVLOG(1) << "StopOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  stopped_ = true;
  current_time_ = base::TimeDelta();
  callback.Run();
}

}  // namespace media
