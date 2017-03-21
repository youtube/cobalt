// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/fake_text_track_stream.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/filters/webvtt_util.h"

namespace media {

FakeTextTrackStream::FakeTextTrackStream()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()), stopping_(false) {}

FakeTextTrackStream::~FakeTextTrackStream() { DCHECK(read_cb_.is_null()); }

void FakeTextTrackStream::Read(const ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());
  DCHECK(read_cb_.is_null());
  OnRead();
  read_cb_ = read_cb;

  if (stopping_) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&FakeTextTrackStream::AbortPendingRead,
                                      base::Unretained(this)));
  }
}

DemuxerStream::Type FakeTextTrackStream::type() const {
  return DemuxerStream::TEXT;
}

bool FakeTextTrackStream::SupportsConfigChanges() { return false; }

VideoRotation FakeTextTrackStream::video_rotation() { return VIDEO_ROTATION_0; }

bool FakeTextTrackStream::enabled() const { return true; }

void FakeTextTrackStream::set_enabled(bool enabled, base::TimeDelta timestamp) {
  NOTIMPLEMENTED();
}

void FakeTextTrackStream::SetStreamStatusChangeCB(
    const StreamStatusChangeCB& cb) {
  NOTIMPLEMENTED();
}

void FakeTextTrackStream::SatisfyPendingRead(const base::TimeDelta& start,
                                             const base::TimeDelta& duration,
                                             const std::string& id,
                                             const std::string& content,
                                             const std::string& settings) {
  DCHECK(!read_cb_.is_null());

  const uint8_t* const data_buf =
      reinterpret_cast<const uint8_t*>(content.data());
  const int data_len = static_cast<int>(content.size());

  std::vector<uint8_t> side_data;
  MakeSideData(id.begin(), id.end(), settings.begin(), settings.end(),
               &side_data);

  const uint8_t* const sd_buf = &side_data[0];
  const int sd_len = static_cast<int>(side_data.size());

  scoped_refptr<DecoderBuffer> buffer;
  buffer = DecoderBuffer::CopyFrom(data_buf, data_len, sd_buf, sd_len);

  buffer->set_timestamp(start);
  buffer->set_duration(duration);

  // Assume all fake text buffers are keyframes.
  buffer->set_is_key_frame(true);

  base::ResetAndReturn(&read_cb_).Run(kOk, buffer);
}

void FakeTextTrackStream::AbortPendingRead() {
  DCHECK(!read_cb_.is_null());
  base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
}

void FakeTextTrackStream::SendEosNotification() {
  DCHECK(!read_cb_.is_null());
  base::ResetAndReturn(&read_cb_).Run(kOk, DecoderBuffer::CreateEOSBuffer());
}

void FakeTextTrackStream::Stop() {
  stopping_ = true;
  if (!read_cb_.is_null()) AbortPendingRead();
}

}  // namespace media
