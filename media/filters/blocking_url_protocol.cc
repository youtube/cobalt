// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/blocking_url_protocol.h"

#include "base/bind.h"
#include "media/base/data_source.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

BlockingUrlProtocol::BlockingUrlProtocol(
    const scoped_refptr<DataSource>& data_source,
    const base::Closure& error_cb)
    : data_source_(data_source),
      error_cb_(error_cb),
      read_event_(false, false),
      read_has_failed_(false),
      last_read_bytes_(0),
      read_position_(0) {
}

BlockingUrlProtocol::~BlockingUrlProtocol() {}

void BlockingUrlProtocol::Abort() {
  SignalReadCompleted(DataSource::kReadError);
}

int BlockingUrlProtocol::Read(int size, uint8* data) {
  // Read errors are unrecoverable.
  if (read_has_failed_)
    return AVERROR(EIO);

  // Even though FFmpeg defines AVERROR_EOF, it's not to be used with I/O
  // routines. Instead return 0 for any read at or past EOF.
  int64 file_size;
  if (data_source_->GetSize(&file_size) && read_position_ >= file_size)
    return 0;

  // Blocking read from data source until |last_read_bytes_| is set and event is
  // signalled.
  data_source_->Read(read_position_, size, data, base::Bind(
      &BlockingUrlProtocol::SignalReadCompleted, base::Unretained(this)));
  read_event_.Wait();

  if (last_read_bytes_ == DataSource::kReadError) {
    // TODO(scherkus): We shouldn't fire |error_cb_| if it was due to Abort().
    error_cb_.Run();
    read_has_failed_ = true;
    return AVERROR(EIO);
  }

  read_position_ += last_read_bytes_;
  return last_read_bytes_;
}

bool BlockingUrlProtocol::GetPosition(int64* position_out) {
  *position_out = read_position_;
  return true;
}

bool BlockingUrlProtocol::SetPosition(int64 position) {
  int64 file_size;
  if ((data_source_->GetSize(&file_size) && position >= file_size) ||
      position < 0) {
    return false;
  }

  read_position_ = position;
  return true;
}

bool BlockingUrlProtocol::GetSize(int64* size_out) {
  return data_source_->GetSize(size_out);
}

bool BlockingUrlProtocol::IsStreaming() {
  return data_source_->IsStreaming();
}

void BlockingUrlProtocol::SignalReadCompleted(int size) {
  last_read_bytes_ = size;
  read_event_.Signal();
}

}  // namespace media
