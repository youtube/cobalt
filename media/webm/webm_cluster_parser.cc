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
      audio_track_num_(audio_track_num),
      audio_default_duration_(audio_default_duration),
      video_track_num_(video_track_num),
      video_default_duration_(video_default_duration),
      video_encryption_key_id_size_(video_encryption_key_id_size),
      parser_(kWebMIdCluster, this),
      last_block_timecode_(-1),
      cluster_timecode_(-1) {
  CHECK_GE(video_encryption_key_id_size, 0);
  if (video_encryption_key_id_size > 0) {
    video_encryption_key_id_.reset(new uint8[video_encryption_key_id_size]);
    memcpy(video_encryption_key_id_.get(), video_encryption_key_id,
           video_encryption_key_id_size);
  }
}

WebMClusterParser::~WebMClusterParser() {}

void WebMClusterParser::Reset() {
  audio_buffers_.clear();
  video_buffers_.clear();
  last_block_timecode_ = -1;
  cluster_timecode_ = -1;
  parser_.Reset();
}

int WebMClusterParser::Parse(const uint8* buf, int size) {
  audio_buffers_.clear();
  video_buffers_.clear();

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
  if (id == kWebMIdCluster)
    cluster_timecode_ = -1;

  return this;
}

bool WebMClusterParser::OnListEnd(int id) {
  if (id == kWebMIdCluster)
    cluster_timecode_ = -1;

  return true;
}

bool WebMClusterParser::OnUInt(int id, int64 val) {
  if (id == kWebMIdTimecode) {
    if (cluster_timecode_ != -1)
      return false;

    cluster_timecode_ = val;
  }

  return true;
}

bool WebMClusterParser::OnSimpleBlock(int track_num, int timecode,
                                      int flags,
                                      const uint8* data, int size) {
  if (cluster_timecode_ == -1) {
    DVLOG(1) << "Got SimpleBlock before cluster timecode.";
    return false;
  }

  if (timecode < 0) {
    DVLOG(1) << "Got SimpleBlock with negative timecode offset " << timecode;
    return false;
  }

  if (last_block_timecode_ != -1 && timecode < last_block_timecode_) {
    DVLOG(1) << "Got SimpleBlock with a timecode before the previous block.";
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

  if (track_num == video_track_num_ && video_encryption_key_id_.get()) {
    buffer->SetDecryptConfig(scoped_ptr<DecryptConfig>(new DecryptConfig(
        video_encryption_key_id_.get(), video_encryption_key_id_size_)));
  }

  buffer->SetTimestamp(timestamp);
  BufferQueue* queue = NULL;

  if (track_num == audio_track_num_) {
    buffer->SetDuration(audio_default_duration_);
    queue = &audio_buffers_;
  } else if (track_num == video_track_num_) {
    buffer->SetDuration(video_default_duration_);
    queue = &video_buffers_;
  } else {
    DVLOG(1) << "Unexpected track number " << track_num;
    return false;
  }

  if (!queue->empty() &&
      buffer->GetTimestamp() == queue->back()->GetTimestamp()) {
    DVLOG(1) << "Got SimpleBlock timecode is not strictly monotonically "
             << "increasing for track " << track_num;
    return false;
  }

  queue->push_back(buffer);
  return true;
}

}  // namespace media
