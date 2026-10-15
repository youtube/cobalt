// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "media/filters/starboard/selected_track_stream.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/chunk_demuxer.h"

namespace media {

SelectedTrackStream::SelectedTrackStream(Type type, ChunkDemuxerStream* stream)
    : type_(type), stream_(stream) {
  DCHECK(stream_);
  DCHECK_EQ(stream_->type(), type_);
}

SelectedTrackStream::~SelectedTrackStream() = default;

void SelectedTrackStream::Read(uint32_t count, ReadCB read_cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!read_cb_);
  read_cb_ = base::BindPostTaskToCurrentDefault(std::move(read_cb));
  read_count_ = count;
  if (is_shutdown_) {
    std::move(read_cb_).Run(kOk, {StreamParserBuffer::CreateEOSBuffer()});
    return;
  }
  if (!holding_reads_) {
    ServeRead_Locked();
  }
}

AudioDecoderConfig SelectedTrackStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  return stream_->audio_decoder_config();
}

VideoDecoderConfig SelectedTrackStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  return stream_->video_decoder_config();
}

DemuxerStream::Type SelectedTrackStream::type() const {
  return type_;
}

StreamLiveness SelectedTrackStream::liveness() const {
  base::AutoLock auto_lock(lock_);
  return stream_->liveness();
}

bool SelectedTrackStream::SupportsConfigChanges() {
  return true;
}

ChunkDemuxerStream* SelectedTrackStream::stream() const {
  base::AutoLock auto_lock(lock_);
  return stream_;
}

void SelectedTrackStream::SwitchTo(ChunkDemuxerStream* stream) {
  base::AutoLock auto_lock(lock_);
  DCHECK(stream);
  DCHECK_EQ(stream->type(), type_);
  if (stream == stream_) {
    return;
  }
  auto [read_cb, count] = stream_->TakePendingRead();
  if (read_cb) {
    DCHECK(!read_cb_);
    read_cb_ = std::move(read_cb);
    read_count_ = count;
  }
  stream_ = stream;
  holding_reads_ = true;
  config_change_pending_ = true;
}

void SelectedTrackStream::AbortReads() {
  base::AutoLock auto_lock(lock_);
  if (read_cb_) {
    std::move(read_cb_).Run(kAborted, {});
  }
}

void SelectedTrackStream::StartReturningData() {
  base::AutoLock auto_lock(lock_);
  holding_reads_ = false;
  if (read_cb_ && !is_shutdown_) {
    ServeRead_Locked();
  }
}

void SelectedTrackStream::Shutdown() {
  base::AutoLock auto_lock(lock_);
  is_shutdown_ = true;
  if (read_cb_) {
    std::move(read_cb_).Run(kOk, {StreamParserBuffer::CreateEOSBuffer()});
  }
}

void SelectedTrackStream::ServeRead_Locked() {
  DCHECK(read_cb_);
  if (config_change_pending_) {
    config_change_pending_ = false;
    std::move(read_cb_).Run(kConfigChanged, {});
    return;
  }
  // |read_cb_| posts its result, so |stream_| can't call back into this
  // object synchronously.
  stream_->Read(read_count_, std::move(read_cb_));
}

}  // namespace media
