// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_cluster_parser.h"

#include <vector>

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/webm/webm_constants.h"

namespace media {

// Generates a 16 byte CTR counter block. The CTR counter block format is a
// CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
// |iv_size| is the size of |iv| in btyes. Returns a string of
// kDecryptionKeySize bytes.
static std::string GenerateCounterBlock(const uint8* iv, int iv_size) {
  std::string counter_block(reinterpret_cast<const char*>(iv), iv_size);
  counter_block.append(DecryptConfig::kDecryptionKeySize - iv_size, 0);
  return counter_block;
}

namespace {

uint32 ReadInteger(const uint8* buf, int size) {
  // Read in the big-endian integer.
  uint32 value = 0;
  for (int i = 0; i < size; ++i)
    value = (value << 8) | buf[i];
  return value;
}

bool ExtractSubsamples(const uint8* buf,
                       size_t frame_data_size,
                       size_t num_partitions,
                       std::vector<SubsampleEntry>* subsample_entries) {
  subsample_entries->clear();
  uint32 clear_bytes = 0;
  // Partition is the wall between alternating sections. Partition offsets are
  // relative to the start of the actual frame data.
  // Size of clear/cipher sections can be calculated from the difference between
  // adjacent partition offsets.
  // Here is an example with 4 partitions (5 sections):
  //   "clear |1 cipher |2 clear |3 cipher |4 clear"
  // With the first and the last implicit partition included:
  //   "|0 clear |1 cipher |2 clear |3 cipher |4 clear |5"
  //   where partition_offset_0 = 0, partition_offset_5 = frame_data_size
  // There are three subsamples in the above example:
  //   Subsample0.clear_bytes = partition_offset_1 - partition_offset_0
  //   Subsample0.cypher_bytes = partition_offset_2 - partition_offset_1
  //   ...
  //   Subsample2.clear_bytes = partition_offset_5 - partition_offset_4
  //   Subsample2.cypher_bytes = 0
  uint32 partition_offset = 0;
  for (size_t i = 0, offset = 0; i <= num_partitions; ++i) {
    const uint32 prev_partition_offset = partition_offset;
    partition_offset =
        (i == num_partitions)
            ? frame_data_size
            : ReadInteger(buf + offset, kWebMEncryptedFramePartitionOffsetSize);
    offset += kWebMEncryptedFramePartitionOffsetSize;
    if (partition_offset < prev_partition_offset) {
      DVLOG(1) << "Partition should not be decreasing " << prev_partition_offset
               << " " << partition_offset;
      return false;
    }

    uint32 cypher_bytes = 0;
    bool new_subsample_entry = false;
    // Alternating clear and cipher sections.
    if ((i % 2) == 0) {
      clear_bytes = partition_offset - prev_partition_offset;
      // Generate a new subsample when finishing reading partition offsets.
      new_subsample_entry = i == num_partitions;
    } else {
      cypher_bytes = partition_offset - prev_partition_offset;
      // Generate a new subsample after seeing a cipher section.
      new_subsample_entry = true;
    }

    if (new_subsample_entry) {
      if (clear_bytes == 0 && cypher_bytes == 0) {
        DVLOG(1) << "Not expecting >2 partitions with the same offsets.";
        return false;
      }
      SubsampleEntry entry;
      entry.clear_bytes = clear_bytes;
      entry.cypher_bytes = cypher_bytes;
      subsample_entries->push_back(entry);
    }
  }
  return true;
}

}  // namespace

WebMClusterParser::WebMClusterParser(
    int64 timecode_scale, int audio_track_num, int video_track_num,
    const std::string& audio_encryption_key_id,
    const std::string& video_encryption_key_id,
    const LogCB& log_cb)
    : timecode_multiplier_(timecode_scale / 1000.0),
      audio_encryption_key_id_(audio_encryption_key_id),
      video_encryption_key_id_(video_encryption_key_id),
      parser_(kWebMIdCluster, this),
      last_block_timecode_(-1),
      block_data_size_(-1),
      block_duration_(-1),
      cluster_timecode_(-1),
      cluster_start_time_(kNoTimestamp()),
      cluster_ended_(false),
      audio_(audio_track_num),
      video_(video_track_num),
      log_cb_(log_cb) {
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

  if (result < 0) {
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
    MEDIA_LOG(log_cb_) << "Block missing from BlockGroup.";
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
    MEDIA_LOG(log_cb_) << "TrackNumber over 127 not supported";
    return false;
  }

  int track_num = buf[0] & 0x7f;
  int timecode = buf[1] << 8 | buf[2];
  int flags = buf[3] & 0xff;
  int lacing = (flags >> 1) & 0x3;

  if (lacing) {
    MEDIA_LOG(log_cb_) << "Lacing " << lacing << " is not supported yet.";
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
    MEDIA_LOG(log_cb_) << "More than 1 Block in a BlockGroup is not supported.";
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
  DCHECK_GE(size, 0);
  if (cluster_timecode_ == -1) {
    MEDIA_LOG(log_cb_) << "Got a block before cluster timecode.";
    return false;
  }

  if (timecode < 0) {
    MEDIA_LOG(log_cb_) << "Got a block with negative timecode offset "
                       << timecode;
    return false;
  }

  if (last_block_timecode_ != -1 && timecode < last_block_timecode_) {
    MEDIA_LOG(log_cb_)
        << "Got a block with a timecode before the previous block.";
    return false;
  }

  Track* track = NULL;
  std::string encryption_key_id;
  if (track_num == audio_.track_num()) {
    track = &audio_;
    encryption_key_id = audio_encryption_key_id_;
  } else if (track_num == video_.track_num()) {
    track = &video_;
    encryption_key_id = video_encryption_key_id_;
  } else {
    MEDIA_LOG(log_cb_) << "Unexpected track number " << track_num;
    return false;
  }

  last_block_timecode_ = timecode;

  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      (cluster_timecode_ + timecode) * timecode_multiplier_);

  // The first bit of the flags is set when the block contains only keyframes.
  // http://www.matroska.org/technical/specs/index.html
  bool is_keyframe = (flags & 0x80) != 0;

#if defined(__LB_SHELL__) || defined(COBALT)
  scoped_refptr<StreamParserBuffer> buffer;
  // Create buffer and copy data over for non-encrypted data. Encrypted data
  // will be handled in the following if block.
  if (encryption_key_id.empty())
    buffer = StreamParserBuffer::CopyFrom(data, size, is_keyframe);
#else  // defined(__LB_SHELL__) || defined(COBALT)
  scoped_refptr<StreamParserBuffer> buffer =
      StreamParserBuffer::CopyFrom(data, size, is_keyframe);
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  if (!buffer) {
    DLOG(WARNING) << "Failed to create StreamParserBuffer";
    return false;
  }

  // Every encrypted Block has a signal byte and IV prepended to it. Current
  // encrypted WebM request for comments specification is here
  // http://wiki.webmproject.org/encryption/webm-encryption-rfc
  if (!encryption_key_id.empty()) {
    DCHECK_EQ(kWebMSignalByteSize, 1);
    if (size < kWebMSignalByteSize) {
      MEDIA_LOG(log_cb_)
          << "Got a block from an encrypted stream with no data.";
      return false;
    }
    const uint8 signal_byte = data[0];
    int data_offset = sizeof(signal_byte);

    // Setting the DecryptConfig object of the buffer while leaving the
    // initialization vector empty will tell the decryptor that the frame is
    // unencrypted.
    std::string counter_block;
    std::vector<SubsampleEntry> subsample_entries;

    if (signal_byte & kWebMFlagEncryptedFrame) {
      if (size < kWebMSignalByteSize + kWebMIvSize) {
        MEDIA_LOG(log_cb_) << "Got an encrypted block with not enough data "
                           << size;
        return false;
      }
      counter_block = GenerateCounterBlock(data + data_offset, kWebMIvSize);
      data_offset += kWebMIvSize;

      if (signal_byte & kWebMFlagEncryptedFramePartitioned) {
        if (size < data_offset + kWebMEncryptedFrameNumPartitionsSize) {
          DVLOG(1) << "Got a partitioned encrypted block with not enough data "
                   << size;
          return false;
        }

        const size_t num_partitions = data[data_offset];
        if (num_partitions == 0) {
          DVLOG(1) << "Got a partitioned encrypted block with 0 partitions.";
          return false;
        }
        data_offset += kWebMEncryptedFrameNumPartitionsSize;
        const uint8* partition_data_start = data + data_offset;
        data_offset += kWebMEncryptedFramePartitionOffsetSize * num_partitions;
        if (size <= data_offset) {
          DVLOG(1) << "Got a partitioned encrypted block with "
                   << num_partitions << " partitions but not enough data "
                   << size;
          return false;
        }
        const size_t frame_data_size = size - data_offset;
        if (!ExtractSubsamples(partition_data_start, frame_data_size,
                               num_partitions, &subsample_entries)) {
          return false;
        }
      }
    }

#if defined(__LB_SHELL__) || defined(COBALT)
    // Don't copy prepended meta data as it is not used by the decrytor and
    // decoder.
    buffer = StreamParserBuffer::CopyFrom(data + data_offset,
                                          size - data_offset, is_keyframe);
#endif  // defined(__LB_SHELL__) || defined(COBALT)
    if (!buffer) {
      DLOG(WARNING) << "Failed to create StreamParserBuffer";
      return false;
    }

    // TODO(fgalligan): Revisit if DecryptConfig needs to be set on unencrypted
    // frames after the CDM API is finalized.
    // Unencrypted frames of potentially encrypted streams currently set
    // DecryptConfig.
    buffer->SetDecryptConfig(scoped_ptr<DecryptConfig>(
        new DecryptConfig(encryption_key_id, counter_block,
#if !defined(__LB_SHELL__) && !defined(COBALT)
                          data_offset,
#endif  // !defined(__LB_SHELL__) && !defined(COBALT)
                          subsample_entries)));
  }

  buffer->SetTimestamp(timestamp);
  if (cluster_start_time_ == kNoTimestamp())
    cluster_start_time_ = timestamp;

  if (block_duration >= 0) {
    buffer->SetDuration(base::TimeDelta::FromMicroseconds(
        block_duration * timecode_multiplier_));
  }

  return track->AddBuffer(buffer);
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
