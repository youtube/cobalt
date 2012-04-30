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
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::InitializeOnDecoderThread,
                 this, make_scoped_refptr(demuxer_stream),
                 status_cb, statistics_cb));
}

void VideoFrameGenerator::Read(const ReadCB& read_cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::ReadOnDecoderThread, this, read_cb));
}

void VideoFrameGenerator::Flush(const base::Closure& flush_cb) {
  message_loop_proxy_->PostTask(FROM_HERE, flush_cb);
}

const gfx::Size& VideoFrameGenerator::natural_size() {
  return natural_size_;
}

void VideoFrameGenerator::Stop(const base::Closure& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::StopOnDecoderThread, this, callback));
}

void VideoFrameGenerator::InitializeOnDecoderThread(
    DemuxerStream* demuxer_stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DVLOG(1) << "InitializeOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  status_cb.Run(PIPELINE_OK);
  stopped_ = false;
}

void VideoFrameGenerator::ReadOnDecoderThread(const ReadCB& read_cb) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  CHECK(!read_cb.is_null());
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

  read_cb.Run(kOk, video_frame);
}

void VideoFrameGenerator::StopOnDecoderThread(const base::Closure& callback) {
  DVLOG(1) << "StopOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  stopped_ = true;
  current_time_ = base::TimeDelta();
  callback.Run();
}

}  // namespace media
