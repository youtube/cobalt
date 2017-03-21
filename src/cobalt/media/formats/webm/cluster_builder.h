// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_
#define COBALT_MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace media {

class Cluster {
 public:
  Cluster(scoped_array<uint8_t> data, int size);
  ~Cluster();

  const uint8_t* data() const { return data_.get(); }
  int size() const { return size_; }

 private:
  scoped_array<uint8_t> data_;
  int size_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Cluster);
};

class ClusterBuilder {
 public:
  ClusterBuilder();
  ~ClusterBuilder();

  void SetClusterTimecode(int64_t cluster_timecode);
  void AddSimpleBlock(int track_num, int64_t timecode, int flags,
                      const uint8_t* data, int size);
  void AddBlockGroup(int track_num, int64_t timecode, int duration, int flags,
                     bool is_key_frame, const uint8_t* data, int size);
  void AddBlockGroupWithoutBlockDuration(int track_num, int64_t timecode,
                                         int flags, bool is_key_frame,
                                         const uint8_t* data, int size);

  scoped_ptr<Cluster> Finish();
  scoped_ptr<Cluster> FinishWithUnknownSize();

 private:
  void AddBlockGroupInternal(int track_num, int64_t timecode,
                             bool include_block_duration, int duration,
                             int flags, bool is_key_frame, const uint8_t* data,
                             int size);
  void Reset();
  void ExtendBuffer(int bytes_needed);
  void UpdateUInt64(int offset, int64_t value);
  void WriteBlock(uint8_t* buf, int track_num, int64_t timecode, int flags,
                  const uint8_t* data, int size);

  scoped_array<uint8_t> buffer_;
  int buffer_size_;
  int bytes_used_;
  int64_t cluster_timecode_;

  DISALLOW_COPY_AND_ASSIGN(ClusterBuilder);
};

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_
