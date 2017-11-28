/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/base/endian_util.h"
#include "media/filters/shell_au.h"
#include "media/filters/shell_parser.h"

namespace {

using namespace media;

bool ReadBytes(uint64 offset,
               size_t size,
               uint8* buffer,
               ShellDataSourceReader* reader) {
  if (reader->BlockingRead(offset, size, buffer) != size) {
    DLOG(ERROR) << "unable to download AU";
    return false;
  }
  return true;
}

// ==== ShellEndOfStreamAU ==================================================

class ShellEndOfStreamAU : public media::ShellAU {
 public:
  ShellEndOfStreamAU(Type type, TimeDelta timestamp)
      : type_(type), timestamp_(timestamp), duration_(kInfiniteDuration()) {}

 private:
  bool IsEndOfStream() const override { return true; }
  bool IsValid() const override { return true; }
  bool Read(ShellDataSourceReader* reader,
            media::DecoderBuffer* buffer) override {
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

// ==== ShellAudioAU =======================================================

class ShellAudioAU : public media::ShellAU {
 public:
  ShellAudioAU(uint64 offset,
               size_t size,
               size_t prepend_size,
               bool is_keyframe,
               TimeDelta timestamp,
               TimeDelta duration,
               ShellParser* parser);

 private:
  bool IsEndOfStream() const override { return false; }
  bool IsValid() const override {
    return offset_ != 0 && size_ != 0 && timestamp_ != media::kNoTimestamp();
  }
  bool Read(ShellDataSourceReader* reader, DecoderBuffer* buffer) override;
  Type GetType() const override { return media::DemuxerStream::AUDIO; }
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
  media::ShellParser* parser_;
};

ShellAudioAU::ShellAudioAU(uint64 offset,
                           size_t size,
                           size_t prepend_size,
                           bool is_keyframe,
                           TimeDelta timestamp,
                           TimeDelta duration,
                           ShellParser* parser)
    : offset_(offset),
      size_(size),
      prepend_size_(prepend_size),
      is_keyframe_(is_keyframe),
      timestamp_(timestamp),
      duration_(duration),
      parser_(parser) {}

bool ShellAudioAU::Read(ShellDataSourceReader* reader, DecoderBuffer* buffer) {
  DCHECK_LE(size_ + prepend_size_, buffer->GetDataSize());
  if (!ReadBytes(offset_, size_, buffer->GetWritableData() + prepend_size_,
                 reader))
    return false;

  if (!parser_->Prepend(this, buffer)) {
    DLOG(ERROR) << "prepend fail";
    return false;
  }

  return true;
}

// ==== ShellVideoAU =======================================================

class ShellVideoAU : public media::ShellAU {
 public:
  ShellVideoAU(uint64 offset,
               size_t size,
               size_t prepend_size,
               uint8 length_of_nalu_size,
               bool is_keyframe,
               TimeDelta timestamp,
               TimeDelta duration,
               ShellParser* parser);

 private:
  bool IsEndOfStream() const override { return false; }
  bool IsValid() const override {
    return offset_ != 0 && size_ != 0 && timestamp_ != media::kNoTimestamp();
  }
  bool Read(ShellDataSourceReader* reader, DecoderBuffer* buffer) override;
  Type GetType() const override { return media::DemuxerStream::VIDEO; }
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
  media::ShellParser* parser_;
};

ShellVideoAU::ShellVideoAU(uint64 offset,
                           size_t size,
                           size_t prepend_size,
                           uint8 length_of_nalu_size,
                           bool is_keyframe,
                           TimeDelta timestamp,
                           TimeDelta duration,
                           ShellParser* parser)
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

bool ShellVideoAU::Read(ShellDataSourceReader* reader, DecoderBuffer* buffer) {
  size_t au_left = size_;                        // bytes left in the AU
  uint64 au_offset = offset_;                    // offset to read in the reader
  size_t buf_left = buffer->GetAllocatedSize();  // bytes left in the buffer
  // The current write position in the buffer
  uint8* buf = buffer->GetWritableData() + prepend_size_;

  // The NALU is stored as [size][data][size][data].... We are going to
  // transform it into [start code][data][start code][data]....
  // The length of size is indicated by length_of_nalu_size_
  while (au_left >= length_of_nalu_size_ && buf_left >= kAnnexBStartCode) {
    uint8 size_buf[4];
    uint32 nal_size;

    // Store [start code]
    endian_util::store_uint32_big_endian(kAnnexBStartCode, buf);
    buf += kAnnexBStartCodeSize;
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

    if (au_left < nal_size || buf_left < nal_size)
      break;

    // Read the [data] from reader into buf
    if (!ReadBytes(au_offset, nal_size, buf, reader))
      return false;

    buf += nal_size;
    au_offset += nal_size;
    au_left -= nal_size;
    buf_left -= nal_size;
  }

  if (au_left != 0) {
    DLOG(ERROR) << "corrupted NALU";
    return false;
  }

  size_ = buf - buffer->GetWritableData();
  buffer->ShrinkTo(size_);

  if (!parser_->Prepend(this, buffer)) {
    DLOG(ERROR) << "prepend fail";
    return false;
  }

  return true;
}

}  // namespace

namespace media {

// ==== ShellAU ================================================================

ShellAU::ShellAU() {}

ShellAU::~ShellAU() {}

// static
scoped_refptr<ShellAU> ShellAU::CreateEndOfStreamAU(DemuxerStream::Type type,
                                                    TimeDelta timestamp) {
  return new ShellEndOfStreamAU(type, timestamp);
}

// static
scoped_refptr<ShellAU> ShellAU::CreateAudioAU(uint64 offset,
                                              size_t size,
                                              size_t prepend_size,
                                              bool is_keyframe,
                                              TimeDelta timestamp,
                                              TimeDelta duration,
                                              ShellParser* parser) {
  return new ShellAudioAU(offset, size, prepend_size, is_keyframe, timestamp,
                          duration, parser);
}

// static
scoped_refptr<ShellAU> ShellAU::CreateVideoAU(uint64 offset,
                                              size_t size,
                                              size_t prepend_size,
                                              uint8 length_of_nalu_size,
                                              bool is_keyframe,
                                              TimeDelta timestamp,
                                              TimeDelta duration,
                                              ShellParser* parser) {
  return new ShellVideoAU(offset, size, prepend_size, length_of_nalu_size,
                          is_keyframe, timestamp, duration, parser);
}

}  // namespace media
