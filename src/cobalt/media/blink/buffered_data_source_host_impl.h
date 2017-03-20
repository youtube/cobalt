// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_BUFFERED_DATA_SOURCE_HOST_IMPL_H_
#define COBALT_MEDIA_BLINK_BUFFERED_DATA_SOURCE_HOST_IMPL_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "base/time.h"
#include "cobalt/media/base/ranges.h"
#include "cobalt/media/blink/interval_map.h"
#include "cobalt/media/blink/media_blink_export.h"

namespace media {

// Interface for testing purposes.
class MEDIA_BLINK_EXPORT BufferedDataSourceHost {
 public:
  // Notify the host of the total size of the media file.
  virtual void SetTotalBytes(int64_t total_bytes) = 0;

  // Notify the host that byte range [start,end] has been buffered.
  // TODO(fischman): remove this method when demuxing is push-based instead of
  // pull-based.  http://crbug.com/131444
  virtual void AddBufferedByteRange(int64_t start, int64_t end) = 0;

 protected:
  virtual ~BufferedDataSourceHost() {}
};

// Provides an implementation of BufferedDataSourceHost that translates the
// buffered byte ranges into estimated time ranges.
class MEDIA_BLINK_EXPORT BufferedDataSourceHostImpl
    : public BufferedDataSourceHost {
 public:
  BufferedDataSourceHostImpl();
  ~BufferedDataSourceHostImpl() OVERRIDE;

  // BufferedDataSourceHost implementation.
  void SetTotalBytes(int64_t total_bytes) OVERRIDE;
  void AddBufferedByteRange(int64_t start, int64_t end) OVERRIDE;

  // Translate the byte ranges to time ranges and append them to the list.
  // TODO(sandersd): This is a confusing name, find something better.
  void AddBufferedTimeRanges(Ranges<base::TimeDelta>* buffered_time_ranges,
                             base::TimeDelta media_duration) const;

  bool DidLoadingProgress();

 private:
  // Total size of the data source.
  int64_t total_bytes_;

  // List of buffered byte ranges for estimating buffered time.
  // The InterValMap value is 1 for bytes that are buffered, 0 otherwise.
  IntervalMap<int64_t, int> buffered_byte_ranges_;

  // True when AddBufferedByteRange() has been called more recently than
  // DidLoadingProgress().
  bool did_loading_progress_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceHostImpl);
};

}  // namespace media

#endif  // COBALT_MEDIA_BLINK_BUFFERED_DATA_SOURCE_HOST_IMPL_H_
