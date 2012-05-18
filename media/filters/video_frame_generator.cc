// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_frame_generator.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_frame.h"

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

void VideoFrameGenerator::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::InitializeOnDecoderThread,
                 this, stream, status_cb, statistics_cb));
}

void VideoFrameGenerator::Read(const ReadCB& read_cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::ReadOnDecoderThread, this, read_cb));
}

void VideoFrameGenerator::Reset(const base::Closure& closure) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::ResetOnDecoderThread, this, closure));
}

void VideoFrameGenerator::Stop(const base::Closure& closure) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameGenerator::StopOnDecoderThread, this, closure));
}

const gfx::Size& VideoFrameGenerator::natural_size() {
  return natural_size_;
}

VideoFrameGenerator::~VideoFrameGenerator() {}

void VideoFrameGenerator::InitializeOnDecoderThread(
    const scoped_refptr<DemuxerStream>& /* stream */,
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

void VideoFrameGenerator::ResetOnDecoderThread(const base::Closure& closure) {
  DVLOG(1) << "ResetOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  current_time_ = base::TimeDelta();
  closure.Run();
}

void VideoFrameGenerator::StopOnDecoderThread(const base::Closure& closure) {
  DVLOG(1) << "StopOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  stopped_ = true;
  current_time_ = base::TimeDelta();
  closure.Run();
}

}  // namespace media
