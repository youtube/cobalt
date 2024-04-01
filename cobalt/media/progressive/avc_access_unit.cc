// Copyright 2012 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/progressive/avc_access_unit.h"

#include <algorithm>

#include "cobalt/media/base/endian_util.h"
#include "cobalt/media/progressive/progressive_parser.h"
#include "third_party/chromium/media/base/decoder_buffer.h"
#include "third_party/chromium/media/base/timestamp_constants.h"

namespace cobalt {
namespace media {

namespace {

using ::media::DecoderBuffer;
using ::media::DemuxerStream;

bool ReadBytes(uint64 offset, size_t size, uint8* buffer,
               DataSourceReader* reader) {
  if (reader->BlockingRead(offset, size, buffer) != size) {
    DLOG(ERROR) << "unable to download AU";
    return false;
  }
  return true;
}

// ==== EndOfStreamAU ==================================================

class EndOfStreamAU : public AvcAccessUnit {
 public:
  EndOfStreamAU(Type type, TimeDelta timestamp)
      : type_(type),
        timestamp_(timestamp),
        duration_(::media::kInfiniteDuration) {}

 private:
  bool IsEndOfStream() const override { return true; }
  bool IsValid() const override { return true; }
  bool Read(DataSourceReader* reader, DecoderBuffer* buffer) override {
    NOTREACHED();
    return false;
  }
  Type GetType() const override { return type_; }
  bool IsKeyframe() const override {
    NOTREACHED();
    return false;
  }
  bool AddPrepend() const override {
    NOTREACHED();
    return false;
  }
  size_t GetSize() const override { return 0; }
  size_t GetMaxSize() const override { return 0; }
  TimeDelta GetTimestamp() const override { return timestamp_; }
  TimeDelta GetDuration() const override { return duration_; }
  void SetDuration(TimeDelta duration) override { duration_ = duration; }
  void SetTimestamp(TimeDelta timestamp) override { timestamp_ = timestamp; }

  Type type_;
  TimeDelta timestamp_;
  TimeDelta duration_;
};

// ==== AudioAU =======================================================

class AudioAU : public AvcAccessUnit {
 public:
  AudioAU(uint64 offset, size_t size, size_t prepend_size, bool is_keyframe,
          TimeDelta timestamp, TimeDelta duration, ProgressiveParser* parser);

 private:
  bool IsEndOfStream() const override { return false; }
  bool IsValid() const override {
    return offset_ != 0 && size_ != 0 && timestamp_ != ::media::kNoTimestamp;
  }
  bool Read(DataSourceReader* reader, DecoderBuffer* buffer) override;
  Type GetType() const override { return DemuxerStream::AUDIO; }
  bool IsKeyframe() const override { return is_keyframe_; }
  bool AddPrepend() const override { return true; }
  size_t GetSize() const override { return size_; }
  size_t GetMaxSize() const override { return size_ + prepend_size_; }
  TimeDelta GetTimestamp() const override { return timestamp_; }
  TimeDelta GetDuration() const override { return duration_; }
  void SetDuration(TimeDelta duration) override { duration_ = duration; }
  void SetTimestamp(TimeDelta timestamp) override { timestamp_ = timestamp; }

  uint64 offset_;
  size_t size_;
  size_t prepend_size_;
  bool is_keyframe_;
  TimeDelta timestamp_;
  TimeDelta duration_;
  ProgressiveParser* parser_;
};

AudioAU::AudioAU(uint64 offset, size_t size, size_t prepend_size,
                 bool is_keyframe, TimeDelta timestamp, TimeDelta duration,
                 ProgressiveParser* parser)
    : offset_(offset),
      size_(size),
      prepend_size_(prepend_size),
      is_keyframe_(is_keyframe),
      timestamp_(timestamp),
      duration_(duration),
      parser_(parser) {}

bool AudioAU::Read(DataSourceReader* reader, DecoderBuffer* buffer) {
  DCHECK_LE(size_ + prepend_size_, buffer->data_size());
  if (!ReadBytes(offset_, size_, buffer->writable_data() + prepend_size_,
                 reader))
    return false;

  if (!parser_->Prepend(this, buffer)) {
    DLOG(ERROR) << "prepend fail";
    return false;
  }

  return true;
}

// ==== VideoAU =======================================================

class VideoAU : public AvcAccessUnit {
 public:
  VideoAU(uint64 offset, size_t size, size_t prepend_size,
          uint8 length_of_nalu_size, bool is_keyframe, TimeDelta timestamp,
          TimeDelta duration, ProgressiveParser* parser);

 private:
  bool IsEndOfStream() const override { return false; }
  bool IsValid() const override {
    return offset_ != 0 && size_ != 0 && timestamp_ != ::media::kNoTimestamp;
  }
  bool Read(DataSourceReader* reader, DecoderBuffer* buffer) override;
  Type GetType() const override { return DemuxerStream::VIDEO; }
  bool IsKeyframe() const override { return is_keyframe_; }
  bool AddPrepend() const override { return is_keyframe_; }
  size_t GetSize() const override { return size_; }
  size_t GetMaxSize() const override {
    // TODO : This code is a proof of concept. It should be fixed
    // with more reasonable value once we have enough data.
    return size_ + prepend_size_ + size_ / 1024 + 1024;
  }
  TimeDelta GetTimestamp() const override { return timestamp_; }
  TimeDelta GetDuration() const override { return duration_; }
  void SetDuration(TimeDelta duration) override { duration_ = duration; }
  void SetTimestamp(TimeDelta timestamp) override { timestamp_ = timestamp; }

  uint64 offset_;
  size_t size_;
  size_t prepend_size_;
  uint8 length_of_nalu_size_;
  bool is_keyframe_;
  TimeDelta timestamp_;
  TimeDelta duration_;
  ProgressiveParser* parser_;
};

VideoAU::VideoAU(uint64 offset, size_t size, size_t prepend_size,
                 uint8 length_of_nalu_size, bool is_keyframe,
                 TimeDelta timestamp, TimeDelta duration,
                 ProgressiveParser* parser)
    : offset_(offset),
      size_(size),
      prepend_size_(prepend_size),
      length_of_nalu_size_(length_of_nalu_size),
      is_keyframe_(is_keyframe),
      timestamp_(timestamp),
      duration_(duration),
      parser_(parser) {
  CHECK_LE(length_of_nalu_size_, 4);
  CHECK_NE(length_of_nalu_size_, 3);
}

bool VideoAU::Read(DataSourceReader* reader, DecoderBuffer* buffer) {
  size_t au_left = size_;                 // bytes left in the AU
  uint64 au_offset = offset_;             // offset to read in the reader
  size_t buf_left = buffer->data_size();  // bytes left in the buffer
  // The current write position in the buffer
  int64_t decoder_buffer_offset = prepend_size_;

  // The NALU is stored as [size][data][size][data].... We are going to
  // transform it into [start code][data][start code][data]....
  // The length of size is indicated by length_of_nalu_size_
  while (au_left >= length_of_nalu_size_ && buf_left >= kAnnexBStartCodeSize) {
    uint8 size_buf[4];
    uint32 nal_size;

    // Store [start code]
    memcpy(buffer->writable_data() + decoder_buffer_offset, kAnnexBStartCode,
           kAnnexBStartCodeSize);
    decoder_buffer_offset += kAnnexBStartCodeSize;
    buf_left -= kAnnexBStartCodeSize;

    // Read [size]
    if (!ReadBytes(au_offset, length_of_nalu_size_, size_buf, reader))
      return false;

    au_offset += length_of_nalu_size_;
    au_left -= length_of_nalu_size_;

    if (length_of_nalu_size_ == 1) {
      nal_size = size_buf[0];
    } else if (length_of_nalu_size_ == 2) {
      nal_size = endian_util::load_uint16_big_endian(size_buf);
    } else {
      DCHECK_EQ(length_of_nalu_size_, 4);
      nal_size = endian_util::load_uint32_big_endian(size_buf);
    }

    if (au_left < nal_size || buf_left < nal_size) break;

    // Read the [data] from reader into buf
    if (!ReadBytes(au_offset, nal_size,
                   buffer->writable_data() + decoder_buffer_offset, reader)) {
      return false;
    }

    decoder_buffer_offset += nal_size;
    au_offset += nal_size;
    au_left -= nal_size;
    buf_left -= nal_size;
  }

  if (au_left != 0) {
    DLOG(ERROR) << "corrupted NALU";
    return false;
  }

  size_ = decoder_buffer_offset;
  buffer->shrink_to(size_);

  if (!parser_->Prepend(this, buffer)) {
    DLOG(ERROR) << "prepend fail";
    return false;
  }

  return true;
}

}  // namespace

// ==== AvcAccessUnit
// ================================================================

AvcAccessUnit::AvcAccessUnit() {}

AvcAccessUnit::~AvcAccessUnit() {}

// static
scoped_refptr<AvcAccessUnit> AvcAccessUnit::CreateEndOfStreamAU(
    DemuxerStream::Type type, TimeDelta timestamp) {
  return new EndOfStreamAU(type, timestamp);
}

// static
scoped_refptr<AvcAccessUnit> AvcAccessUnit::CreateAudioAU(
    uint64 offset, size_t size, size_t prepend_size, bool is_keyframe,
    TimeDelta timestamp, TimeDelta duration, ProgressiveParser* parser) {
  return new AudioAU(offset, size, prepend_size, is_keyframe, timestamp,
                     duration, parser);
}

// static
scoped_refptr<AvcAccessUnit> AvcAccessUnit::CreateVideoAU(
    uint64 offset, size_t size, size_t prepend_size, uint8 length_of_nalu_size,
    bool is_keyframe, TimeDelta timestamp, TimeDelta duration,
    ProgressiveParser* parser) {
  return new VideoAU(offset, size, prepend_size, length_of_nalu_size,
                     is_keyframe, timestamp, duration, parser);
}

}  // namespace media
}  // namespace cobalt
