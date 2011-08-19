// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/cluster_builder.h"

#include "base/logging.h"
#include "media/base/data_buffer.h"

namespace media {

static const uint8 kClusterHeader[] = {
  0x1F, 0x43, 0xB6, 0x75,  // CLUSTER ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // cluster(size = 0)
  0xE7,  // Timecode ID
  0x88,  // timecode(size=8)
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timecode value
};

const int kClusterHeaderSize = sizeof(kClusterHeader);
const int kClusterSizeOffset = 4;
const int kClusterTimecodeOffset = 14;

static const uint8 kSimpleBlockHeader[] = {
  0xA3,  // SimpleBlock ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SimpleBlock(size = 0)
};

const int kSimpleBlockHeaderSize = sizeof(kSimpleBlockHeader);
const int kSimpleBlockSizeOffset = 1;

const int kInitialBufferSize = 32768;

Cluster::Cluster(const uint8* data, int size) : data_(data), size_(size) {}
Cluster::~Cluster() {}

ClusterBuilder::ClusterBuilder() { Reset(); }
ClusterBuilder::~ClusterBuilder() {}

void ClusterBuilder::SetClusterTimecode(int64 cluster_timecode) {
  DCHECK_EQ(cluster_timecode_, -1);

  cluster_timecode_ = cluster_timecode;

  // Write the timecode into the header.
  uint8* buf = buffer_.get() + kClusterTimecodeOffset;
  for (int i = 7; i >= 0; --i) {
    buf[i] = cluster_timecode & 0xff;
    cluster_timecode >>= 8;
  }
}

void ClusterBuilder::AddSimpleBlock(int track_num, int64 timecode, int flags,
                                    const uint8* data, int size) {
  DCHECK_GE(track_num, 0);
  DCHECK_LE(track_num, 126);
  DCHECK_GE(flags, 0);
  DCHECK_LE(flags, 0xff);
  DCHECK(data);
  DCHECK_GT(size, 0);
  DCHECK_NE(cluster_timecode_, -1);

  int64 timecode_delta = timecode - cluster_timecode_;
  DCHECK_GE(timecode_delta, -32768);
  DCHECK_LE(timecode_delta, 32767);

  int block_size = 4 + size;
  int bytes_needed = kSimpleBlockHeaderSize + block_size;
  if (bytes_needed > (buffer_size_ - bytes_used_))
    ExtendBuffer(bytes_needed);

  uint8* buf = buffer_.get() + bytes_used_;
  int block_offset = bytes_used_;
  memcpy(buf, kSimpleBlockHeader, kSimpleBlockHeaderSize);
  UpdateUInt64(block_offset + kSimpleBlockSizeOffset, block_size);
  buf += kSimpleBlockHeaderSize;

  buf[0] = 0x80 | (track_num & 0x7F);
  buf[1] = (timecode_delta >> 8) & 0xff;
  buf[2] = timecode_delta & 0xff;
  buf[3] = flags & 0xff;
  memcpy(buf + 4, data, size);

  bytes_used_ += bytes_needed;
}

Cluster* ClusterBuilder::Finish() {
  DCHECK_NE(cluster_timecode_, -1);

  UpdateUInt64(kClusterSizeOffset, bytes_used_ - (kClusterSizeOffset + 8));

  scoped_ptr<Cluster> ret(new Cluster(buffer_.release(), bytes_used_));
  Reset();
  return ret.release();
}

void ClusterBuilder::Reset() {
  buffer_size_ = kInitialBufferSize;
  buffer_.reset(new uint8[buffer_size_]);
  memcpy(buffer_.get(), kClusterHeader, kClusterHeaderSize);
  bytes_used_ = kClusterHeaderSize;
  cluster_timecode_ = -1;
}

void ClusterBuilder::ExtendBuffer(int bytes_needed) {
  int new_buffer_size = 2 * buffer_size_;

  while ((new_buffer_size - bytes_used_) < bytes_needed)
    new_buffer_size *= 2;

  scoped_array<uint8> new_buffer(new uint8[new_buffer_size]);

  memcpy(new_buffer.get(), buffer_.get(), bytes_used_);
  buffer_.reset(new_buffer.release());
  buffer_size_ = new_buffer_size;
}

void ClusterBuilder::UpdateUInt64(int offset, int64 value) {
  DCHECK_LE(offset + 7, buffer_size_);
  uint8* buf = buffer_.get() + offset;

  // Fill the last 7 bytes of size field in big-endian order.
  for (int i = 7; i > 0; i--) {
    buf[i] = value & 0xff;
    value >>= 8;
  }
}

}  // namespace media
