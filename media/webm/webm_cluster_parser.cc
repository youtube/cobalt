// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_cluster_parser.h"

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/webm/webm_constants.h"

namespace media {

static Buffer* CreateBuffer(const uint8* data, size_t size) {
  scoped_array<uint8> buf(new uint8[size]);
  memcpy(buf.get(), data, size);
  return new DataBuffer(buf.release(), size);
}

WebMClusterParser::WebMClusterParser(int64 timecode_scale,
                                     int audio_track_num,
                                     base::TimeDelta audio_default_duration,
                                     int video_track_num,
                                     base::TimeDelta video_default_duration)
    : timecode_multiplier_(timecode_scale / 1000.0),
      audio_track_num_(audio_track_num),
      audio_default_duration_(audio_default_duration),
      video_track_num_(video_track_num),
      video_default_duration_(video_default_duration),
      last_block_timecode_(-1),
      cluster_timecode_(-1) {
}

WebMClusterParser::~WebMClusterParser() {}

int WebMClusterParser::Parse(const uint8* buf, int size) {
  last_block_timecode_ = -1;
  cluster_timecode_ = -1;
  audio_buffers_.clear();
  video_buffers_.clear();

  return WebMParseListElement(buf, size, kWebMIdCluster, 1, this);
}

bool WebMClusterParser::OnListStart(int id) {
  if (id == kWebMIdCluster)
    cluster_timecode_ = -1;

  return true;
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

bool WebMClusterParser::OnFloat(int id, double val) {
  VLOG(1) << "Unexpected float element with ID " << std::hex << id;
  return false;
}

bool WebMClusterParser::OnBinary(int id, const uint8* data, int size) {
  VLOG(1) << "Unexpected binary element with ID " << std::hex << id;
  return false;
}

bool WebMClusterParser::OnString(int id, const std::string& str) {
  VLOG(1) << "Unexpected string element with ID " << std::hex << id;
  return false;
}

bool WebMClusterParser::OnSimpleBlock(int track_num, int timecode,
                                      int flags,
                                      const uint8* data, int size) {
  if (cluster_timecode_ == -1) {
    VLOG(1) << "Got SimpleBlock before cluster timecode.";
    return false;
  }

  if (timecode < 0) {
    VLOG(1) << "Got SimpleBlock with negative timecode offset " << timecode;
    return false;
  }

  if (last_block_timecode_ != -1 && timecode < last_block_timecode_) {
    VLOG(1) << "Got SimpleBlock with a timecode before the previous block.";
    return false;
  }

  last_block_timecode_ = timecode;

  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      (cluster_timecode_ + timecode) * timecode_multiplier_);

  scoped_refptr<Buffer> buffer(CreateBuffer(data, size));
  buffer->SetTimestamp(timestamp);
  BufferQueue* queue = NULL;

  if (track_num == audio_track_num_) {
    buffer->SetDuration(audio_default_duration_);
    queue = &audio_buffers_;
  } else if (track_num == video_track_num_) {
    buffer->SetDuration(video_default_duration_);
    queue = &video_buffers_;
  } else {
    VLOG(1) << "Unexpected track number " << track_num;
    return false;
  }

  if (!queue->empty() &&
      buffer->GetTimestamp() == queue->back()->GetTimestamp()) {
    VLOG(1) << "Got SimpleBlock timecode is not strictly monotonically "
            << "increasing for track " << track_num;
    return false;
  }

  queue->push_back(buffer);
  return true;
}

}  // namespace media
