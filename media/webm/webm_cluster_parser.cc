// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_cluster_parser.h"

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/webm/webm_constants.h"

namespace media {

WebMClusterParser::WebMClusterParser(int64 timecode_scale,
                                     int audio_track_num,
                                     base::TimeDelta audio_default_duration,
                                     int video_track_num,
                                     base::TimeDelta video_default_duration,
                                     const uint8* video_encryption_key_id,
                                     int video_encryption_key_id_size)
    : timecode_multiplier_(timecode_scale / 1000.0),
      video_encryption_key_id_size_(video_encryption_key_id_size),
      parser_(kWebMIdCluster, this),
      last_block_timecode_(-1),
      block_data_size_(-1),
      block_duration_(-1),
      cluster_timecode_(-1),
      audio_(audio_track_num, audio_default_duration),
      video_(video_track_num, video_default_duration) {
  CHECK_GE(video_encryption_key_id_size, 0);
  if (video_encryption_key_id_size > 0) {
    video_encryption_key_id_.reset(new uint8[video_encryption_key_id_size]);
    memcpy(video_encryption_key_id_.get(), video_encryption_key_id,
           video_encryption_key_id_size);
  }
}

WebMClusterParser::~WebMClusterParser() {}

void WebMClusterParser::Reset() {
  last_block_timecode_ = -1;
  cluster_timecode_ = -1;
  parser_.Reset();
  audio_.Reset();
  video_.Reset();
}

int WebMClusterParser::Parse(const uint8* buf, int size) {
  audio_.ClearBufferQueue();
  video_.ClearBufferQueue();

  int result = parser_.Parse(buf, size);

  if (result <= 0)
    return result;

  if (parser_.IsParsingComplete()) {
    // Reset the parser if we're done parsing so that
    // it is ready to accept another cluster on the next
    // call.
    parser_.Reset();

    last_block_timecode_ = -1;
    cluster_timecode_ = -1;
  }

  return result;
}

WebMParserClient* WebMClusterParser::OnListStart(int id) {
  if (id == kWebMIdCluster) {
    cluster_timecode_ = -1;
  } else if (id == kWebMIdBlockGroup) {
    block_data_.reset();
    block_data_size_ = -1;
    block_duration_ = -1;
  }

  return this;
}

bool WebMClusterParser::OnListEnd(int id) {
  if (id == kWebMIdCluster) {
    cluster_timecode_ = -1;
    return true;
  }

  if (id != kWebMIdBlockGroup)
    return true;

  // Make sure the BlockGroup actually had a Block.
  if (block_data_size_ == -1) {
    DVLOG(1) << "Block missing from BlockGroup.";
    return false;
  }

  bool result = ParseBlock(block_data_.get(), block_data_size_,
                           block_duration_);
  block_data_.reset();
  block_data_size_ = -1;
  block_duration_ = -1;
  return result;
}

bool WebMClusterParser::OnUInt(int id, int64 val) {
  if (id == kWebMIdTimecode) {
    if (cluster_timecode_ != -1)
      return false;

    cluster_timecode_ = val;
  } else if (id == kWebMIdBlockDuration) {
    if (block_duration_ != -1)
      return false;
    block_duration_ = val;
  }

  return true;
}

bool WebMClusterParser::ParseBlock(const uint8* buf, int size, int duration) {
  if (size < 4)
    return false;

  // Return an error if the trackNum > 127. We just aren't
  // going to support large track numbers right now.
  if (!(buf[0] & 0x80)) {
    DVLOG(1) << "TrackNumber over 127 not supported";
    return false;
  }

  int track_num = buf[0] & 0x7f;
  int timecode = buf[1] << 8 | buf[2];
  int flags = buf[3] & 0xff;
  int lacing = (flags >> 1) & 0x3;

  if (lacing) {
    DVLOG(1) << "Lacing " << lacing << " not supported yet.";
    return false;
  }

  // Sign extend negative timecode offsets.
  if (timecode & 0x8000)
    timecode |= (-1 << 16);

  const uint8* frame_data = buf + 4;
  int frame_size = size - (frame_data - buf);
  return OnBlock(track_num, timecode, duration, flags, frame_data, frame_size);
}

bool WebMClusterParser::OnBinary(int id, const uint8* data, int size) {
  if (id == kWebMIdSimpleBlock)
    return ParseBlock(data, size, -1);

  if (id != kWebMIdBlock)
    return true;

  if (block_data_.get()) {
    DVLOG(1) << "More than 1 Block in a BlockGroup is not supported.";
    return false;
  }

  block_data_.reset(new uint8[size]);
  memcpy(block_data_.get(), data, size);
  block_data_size_ = size;
  return true;
}

bool WebMClusterParser::OnBlock(int track_num, int timecode,
                                int  block_duration,
                                int flags,
                                const uint8* data, int size) {
  if (cluster_timecode_ == -1) {
    DVLOG(1) << "Got a block before cluster timecode.";
    return false;
  }

  if (timecode < 0) {
    DVLOG(1) << "Got a block with negative timecode offset " << timecode;
    return false;
  }

  if (last_block_timecode_ != -1 && timecode < last_block_timecode_) {
    DVLOG(1) << "Got a block with a timecode before the previous block.";
    return false;
  }

  last_block_timecode_ = timecode;

  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      (cluster_timecode_ + timecode) * timecode_multiplier_);

  // The first bit of the flags is set when the block contains only keyframes.
  // http://www.matroska.org/technical/specs/index.html
  bool is_keyframe = (flags & 0x80) != 0;
  scoped_refptr<StreamParserBuffer> buffer =
      StreamParserBuffer::CopyFrom(data, size, is_keyframe);

  if (track_num == video_.track_num() && video_encryption_key_id_.get()) {
    buffer->SetDecryptConfig(scoped_ptr<DecryptConfig>(new DecryptConfig(
        video_encryption_key_id_.get(), video_encryption_key_id_size_)));
  }

  buffer->SetTimestamp(timestamp);

  if (block_duration >= 0) {
    buffer->SetDuration(base::TimeDelta::FromMicroseconds(
        block_duration * timecode_multiplier_));
  }

  if (track_num == audio_.track_num()) {
    return audio_.AddBuffer(buffer);
  } else if (track_num == video_.track_num()) {
    return video_.AddBuffer(buffer);
  }

  DVLOG(1) << "Unexpected track number " << track_num;
  return false;
}

WebMClusterParser::Track::Track(int track_num,
                                base::TimeDelta default_duration)
    : track_num_(track_num),
      default_duration_(default_duration) {
}

WebMClusterParser::Track::~Track() {}

bool WebMClusterParser::Track::AddBuffer(
    const scoped_refptr<StreamParserBuffer>& buffer) {
  if (!buffers_.empty() &&
      buffer->GetTimestamp() == buffers_.back()->GetTimestamp()) {
    DVLOG(1) << "Got a block timecode that is not strictly monotonically "
             << "increasing for track " << track_num_;
    return false;
  }

  if (buffer->GetDuration() == kNoTimestamp())
    buffer->SetDuration(default_duration_);

  if (delayed_buffer_) {
    // Update the duration of the delayed buffer and place it into the queue.
    base::TimeDelta new_duration =
        buffer->GetTimestamp() - delayed_buffer_->GetTimestamp();

    if (new_duration <= base::TimeDelta())
      return false;

    delayed_buffer_->SetDuration(new_duration);
    buffers_.push_back(delayed_buffer_);

    delayed_buffer_ = NULL;
  }

  // Place the buffer in delayed buffer slot if we don't know
  // its duration.
  if (buffer->GetDuration() == kNoTimestamp()) {
    delayed_buffer_ = buffer;
    return true;
  }

  buffers_.push_back(buffer);
  return true;
}

void WebMClusterParser::Track::Reset() {
  buffers_.clear();
  delayed_buffer_ = NULL;
}

void WebMClusterParser::Track::ClearBufferQueue() {
  buffers_.clear();
}

}  // namespace media
