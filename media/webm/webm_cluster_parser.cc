// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_cluster_parser.h"

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "media/base/data_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/webm/webm_constants.h"

namespace media {

// Generates a 16 byte CTR counter block. The CTR counter block format is a
// CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
// Always returns a valid pointer to a buffer of kDecryptionKeySize bytes.
static scoped_array<uint8> GenerateCounterBlock(uint64 iv) {
  scoped_array<uint8> counter_block_data(
      new uint8[DecryptConfig::kDecryptionKeySize]);

  // Set the IV.
  memcpy(counter_block_data.get(), &iv, sizeof(iv));

  // Set block counter to all 0's.
  memset(counter_block_data.get() + sizeof(iv),
         0,
         DecryptConfig::kDecryptionKeySize - sizeof(iv));

  return counter_block_data.Pass();
}

WebMClusterParser::WebMClusterParser(int64 timecode_scale,
                                     int audio_track_num,
                                     int video_track_num,
                                     const uint8* video_encryption_key_id,
                                     int video_encryption_key_id_size)
    : timecode_multiplier_(timecode_scale / 1000.0),
      video_encryption_key_id_size_(video_encryption_key_id_size),
      parser_(kWebMIdCluster, this),
      last_block_timecode_(-1),
      block_data_size_(-1),
      block_duration_(-1),
      cluster_timecode_(-1),
      cluster_start_time_(kNoTimestamp()),
      cluster_ended_(false),
      audio_(audio_track_num),
      video_(video_track_num) {
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
  cluster_start_time_ = kNoTimestamp();
  cluster_ended_ = false;
  parser_.Reset();
  audio_.Reset();
  video_.Reset();
}

int WebMClusterParser::Parse(const uint8* buf, int size) {
  audio_.Reset();
  video_.Reset();

  int result = parser_.Parse(buf, size);

  if (result <= 0) {
    cluster_ended_ = false;
    return result;
  }

  cluster_ended_ = parser_.IsParsingComplete();
  if (cluster_ended_) {
    // If there were no buffers in this cluster, set the cluster start time to
    // be the |cluster_timecode_|.
    if (cluster_start_time_ == kNoTimestamp()) {
      DCHECK_GT(cluster_timecode_, -1);
      cluster_start_time_ = base::TimeDelta::FromMicroseconds(
          cluster_timecode_ * timecode_multiplier_);
    }

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
    cluster_start_time_ = kNoTimestamp();
  } else if (id == kWebMIdBlockGroup) {
    block_data_.reset();
    block_data_size_ = -1;
    block_duration_ = -1;
  }

  return this;
}

bool WebMClusterParser::OnListEnd(int id) {
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

  // Every encrypted Block has an HMAC and IV prepended to it. Current encrypted
  // WebM request for comments specification is here
  // http://wiki.webmproject.org/encryption/webm-encryption-rfc
  bool encrypted = track_num == video_.track_num() &&
                   video_encryption_key_id_.get();
  // If encrypted skip past the HMAC. Encrypted buffers must include the IV and
  // the encrypted frame because the decryptor will verify this data before
  // decryption. The HMAC and IV will be copied into DecryptConfig.
  int offset = (encrypted) ? kWebMHmacSize : 0;

  // The first bit of the flags is set when the block contains only keyframes.
  // http://www.matroska.org/technical/specs/index.html
  bool is_keyframe = (flags & 0x80) != 0;
  scoped_refptr<StreamParserBuffer> buffer =
      StreamParserBuffer::CopyFrom(data + offset, size - offset, is_keyframe);

  if (encrypted) {
    uint64 network_iv;
    memcpy(&network_iv, data + kWebMHmacSize, sizeof(network_iv));
    const uint64 iv = base::NetToHost64(network_iv);

    scoped_array<uint8> counter_block(GenerateCounterBlock(iv));
    buffer->SetDecryptConfig(scoped_ptr<DecryptConfig>(new DecryptConfig(
        std::string(
            reinterpret_cast<const char*>(video_encryption_key_id_.get()),
            video_encryption_key_id_size_),
        std::string(
            reinterpret_cast<const char*>(counter_block.get()),
            DecryptConfig::kDecryptionKeySize),
        std::string(reinterpret_cast<const char*>(data), kWebMHmacSize),
        sizeof(iv),
        std::vector<SubsampleEntry>())));
  }

  buffer->SetTimestamp(timestamp);
  if (cluster_start_time_ == kNoTimestamp())
    cluster_start_time_ = timestamp;

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

WebMClusterParser::Track::Track(int track_num)
    : track_num_(track_num) {
}

WebMClusterParser::Track::~Track() {}

bool WebMClusterParser::Track::AddBuffer(
    const scoped_refptr<StreamParserBuffer>& buffer) {
  DVLOG(2) << "AddBuffer() : " << track_num_
           << " ts " << buffer->GetTimestamp().InSecondsF()
           << " dur " << buffer->GetDuration().InSecondsF()
           << " kf " << buffer->IsKeyframe()
           << " size " << buffer->GetDataSize();

  buffers_.push_back(buffer);
  return true;
}

void WebMClusterParser::Track::Reset() {
  buffers_.clear();
}

}  // namespace media
