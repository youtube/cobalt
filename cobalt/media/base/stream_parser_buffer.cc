// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/stream_parser_buffer.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/media/base/timestamp_constants.h"

namespace cobalt {
namespace media {

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new StreamParserBuffer);
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    Allocator* allocator, const uint8_t* data, int data_size, bool is_key_frame,
    Type type, TrackId track_id) {
  scoped_refptr<StreamParserBuffer> stream_parser_buffer =
      make_scoped_refptr(new StreamParserBuffer(allocator, data, data_size,
                                                is_key_frame, type, track_id));
  if (!stream_parser_buffer->has_data()) {
    return NULL;
  }
  return stream_parser_buffer;
}

DecodeTimestamp StreamParserBuffer::GetDecodeTimestamp() const {
  if (decode_timestamp_ == kNoDecodeTimestamp())
    return DecodeTimestamp::FromPresentationTime(timestamp());
  return decode_timestamp_;
}

void StreamParserBuffer::SetDecodeTimestamp(DecodeTimestamp timestamp) {
  decode_timestamp_ = timestamp;
  if (preroll_buffer_.get()) preroll_buffer_->SetDecodeTimestamp(timestamp);
}

StreamParserBuffer::StreamParserBuffer()
    : decode_timestamp_(kNoDecodeTimestamp()),
      config_id_(kInvalidConfigId),
      track_id_(0),
      is_duration_estimated_(false) {}

StreamParserBuffer::StreamParserBuffer(Allocator* allocator,
                                       const uint8_t* data, int data_size,
                                       bool is_key_frame, Type type,
                                       TrackId track_id)
    : DecoderBuffer(allocator, type, data, data_size),
      decode_timestamp_(kNoDecodeTimestamp()),
      config_id_(kInvalidConfigId),
      track_id_(track_id),
      is_duration_estimated_(false) {
  if (allocations().number_of_buffers() == 0) {
    return;
  }
  // TODO(scherkus): Should DataBuffer constructor accept a timestamp and
  // duration to force clients to set them? Today they end up being zero which
  // is both a common and valid value and could lead to bugs.
  if (data) {
    set_duration(kNoTimestamp);
  }

  if (is_key_frame) set_is_key_frame(true);
}

StreamParserBuffer::StreamParserBuffer(Allocator* allocator,
                                       Allocator::Allocations allocations,
                                       bool is_key_frame, Type type,
                                       TrackId track_id)
    : DecoderBuffer(allocator, type, allocations),
      decode_timestamp_(kNoDecodeTimestamp()),
      config_id_(kInvalidConfigId),
      track_id_(track_id),
      is_duration_estimated_(false) {
  // TODO(scherkus): Should DataBuffer constructor accept a timestamp and
  // duration to force clients to set them? Today they end up being zero which
  // is both a common and valid value and could lead to bugs.
  set_duration(kNoTimestamp);

  if (is_key_frame) set_is_key_frame(true);
}

StreamParserBuffer::~StreamParserBuffer() {}

int StreamParserBuffer::GetConfigId() const { return config_id_; }

void StreamParserBuffer::SetConfigId(int config_id) {
  config_id_ = config_id;
  if (preroll_buffer_.get()) preroll_buffer_->SetConfigId(config_id);
}

int StreamParserBuffer::GetSpliceBufferConfigId(size_t index) const {
  return index < splice_buffers().size() ? splice_buffers_[index]->GetConfigId()
                                         : GetConfigId();
}

void StreamParserBuffer::ConvertToSpliceBuffer(
    const BufferQueue& pre_splice_buffers) {
  DCHECK(splice_buffers_.empty());
  DCHECK(duration() > base::TimeDelta())
      << "Only buffers with a valid duration can convert to a splice buffer."
      << " pts " << timestamp().InSecondsF() << " dts "
      << GetDecodeTimestamp().InSecondsF() << " dur "
      << duration().InSecondsF();
  DCHECK(!end_of_stream());

  // Splicing requires non-estimated sample accurate durations to be confident
  // things will sound smooth. Also, we cannot be certain whether estimated
  // overlap is really a splice scenario, or just over estimation.
  DCHECK(!is_duration_estimated_);

  // Make a copy of this first, before making any changes.
  scoped_refptr<StreamParserBuffer> overlapping_buffer = Clone();
  overlapping_buffer->set_splice_timestamp(kNoTimestamp);

  const scoped_refptr<StreamParserBuffer>& first_splice_buffer =
      pre_splice_buffers.front();

  // Ensure the given buffers are actually before the splice point.
  DCHECK(first_splice_buffer->timestamp() <= overlapping_buffer->timestamp());

  // TODO(dalecurtis): We should also clear |data|, but since that implies EOS
  // care must be taken to ensure there are no clients relying on that behavior.

  // Move over any preroll from this buffer.
  if (preroll_buffer_.get()) {
    DCHECK(!overlapping_buffer->preroll_buffer_.get());
    overlapping_buffer->preroll_buffer_.swap(preroll_buffer_);
  }

  // Rewrite |this| buffer as a splice buffer.
  SetDecodeTimestamp(first_splice_buffer->GetDecodeTimestamp());
  SetConfigId(first_splice_buffer->GetConfigId());
  set_timestamp(first_splice_buffer->timestamp());
  set_is_key_frame(first_splice_buffer->is_key_frame());
  DCHECK_EQ(type(), first_splice_buffer->type());
  track_id_ = first_splice_buffer->track_id();
  set_splice_timestamp(overlapping_buffer->timestamp());

  // The splice duration is the duration of all buffers before the splice plus
  // the highest ending timestamp after the splice point.
  DCHECK(overlapping_buffer->duration() > base::TimeDelta());
  DCHECK(pre_splice_buffers.back()->duration() > base::TimeDelta());
  set_duration(
      std::max(overlapping_buffer->timestamp() + overlapping_buffer->duration(),
               pre_splice_buffers.back()->timestamp() +
                   pre_splice_buffers.back()->duration()) -
      first_splice_buffer->timestamp());

  // Copy all pre splice buffers into our wrapper buffer.
  for (BufferQueue::const_iterator it = pre_splice_buffers.begin();
       it != pre_splice_buffers.end(); ++it) {
    const scoped_refptr<StreamParserBuffer>& buffer = *it;
    DCHECK(!buffer->end_of_stream());
    DCHECK(!buffer->preroll_buffer().get());
    DCHECK(buffer->splice_buffers().empty());
    DCHECK(!buffer->is_duration_estimated());
    splice_buffers_.push_back(buffer->Clone());
    splice_buffers_.back()->set_splice_timestamp(splice_timestamp());
  }

  splice_buffers_.push_back(overlapping_buffer);
}

void StreamParserBuffer::SetPrerollBuffer(
    const scoped_refptr<StreamParserBuffer>& preroll_buffer) {
  DCHECK(!preroll_buffer_.get());
  DCHECK(!end_of_stream());
  DCHECK(!preroll_buffer->end_of_stream());
  DCHECK(!preroll_buffer->preroll_buffer_.get());
  DCHECK(preroll_buffer->splice_timestamp() == kNoTimestamp);
  DCHECK(preroll_buffer->splice_buffers().empty());
  DCHECK(preroll_buffer->timestamp() <= timestamp());
  DCHECK(preroll_buffer->discard_padding() == DecoderBuffer::DiscardPadding());
  DCHECK_EQ(preroll_buffer->type(), type());
  DCHECK_EQ(preroll_buffer->track_id(), track_id());

  preroll_buffer_ = preroll_buffer;
  preroll_buffer_->set_timestamp(timestamp());
  preroll_buffer_->SetDecodeTimestamp(GetDecodeTimestamp());

  // Mark the entire buffer for discard.
  preroll_buffer_->set_discard_padding(
      std::make_pair(kInfiniteDuration, base::TimeDelta()));
}

void StreamParserBuffer::set_timestamp(base::TimeDelta timestamp) {
  DecoderBuffer::set_timestamp(timestamp);
  if (preroll_buffer_.get()) preroll_buffer_->set_timestamp(timestamp);
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::Clone() const {
  if (end_of_stream()) {
    return StreamParserBuffer::CreateEOSBuffer();
  }

  scoped_refptr<StreamParserBuffer> clone = new StreamParserBuffer(
      allocator(), allocations(), is_key_frame(), type(), track_id());
  clone->SetDecodeTimestamp(GetDecodeTimestamp());
  clone->SetConfigId(GetConfigId());
  clone->set_timestamp(timestamp());
  clone->set_duration(duration());
  clone->set_is_duration_estimated(is_duration_estimated());
  clone->set_discard_padding(discard_padding());
  clone->set_splice_timestamp(splice_timestamp());
  const DecryptConfig* decrypt_config = this->decrypt_config();
  if (decrypt_config) {
    clone->set_decrypt_config(scoped_ptr<DecryptConfig>(
        new DecryptConfig(decrypt_config->key_id(), decrypt_config->iv(),
                          decrypt_config->subsamples())));
  }

  return clone;
}

}  // namespace media
}  // namespace cobalt
