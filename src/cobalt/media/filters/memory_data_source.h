// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FILTERS_MEMORY_DATA_SOURCE_H_
#define COBALT_MEDIA_FILTERS_MEMORY_DATA_SOURCE_H_

#include "base/basictypes.h"
#include "cobalt/media/base/data_source.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Basic data source that treats the URL as a file path, and uses the file
// system to read data for a media pipeline.
class MEDIA_EXPORT MemoryDataSource : public DataSource {
 public:
  // Construct MemoryDataSource with |data| and |size|. The data is guaranteed
  // to be valid during the lifetime of MemoryDataSource.
  MemoryDataSource(const uint8_t* data, size_t size);
  ~MemoryDataSource() final;

  // Implementation of DataSource.
  void Read(int64_t position, int size, uint8_t* data,
            const DataSource::ReadCB& read_cb) final;
  void Stop() final;
  void Abort() final;
  bool GetSize(int64_t* size_out) final;
  bool IsStreaming() final;
  void SetBitrate(int bitrate) final;

 private:
  const uint8_t* data_ = NULL;
  size_t size_ = 0;

  bool is_stopped_ = false;

  DISALLOW_COPY_AND_ASSIGN(MemoryDataSource);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FILTERS_MEMORY_DATA_SOURCE_H_
