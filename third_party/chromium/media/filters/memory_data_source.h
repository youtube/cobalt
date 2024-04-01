// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MEMORY_DATA_SOURCE_H_
#define MEDIA_FILTERS_MEMORY_DATA_SOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/base/data_source.h"

namespace media {

// Basic data source that treats the URL as a file path, and uses the file
// system to read data for a media pipeline.
class MEDIA_EXPORT MemoryDataSource final : public DataSource {
 public:
  // Construct MemoryDataSource with |data| and |size|. The data is guaranteed
  // to be valid during the lifetime of MemoryDataSource.
  MemoryDataSource(const uint8_t* data, size_t size);

  // Similar to the above, but takes ownership of the std::string.
  explicit MemoryDataSource(std::string data);

  MemoryDataSource(const MemoryDataSource&) = delete;
  MemoryDataSource& operator=(const MemoryDataSource&) = delete;

  ~MemoryDataSource() final;

  // Implementation of DataSource.
  void Read(int64_t position,
            int size,
            uint8_t* data,
            DataSource::ReadCB read_cb) final;
  void Stop() final;
  void Abort() final;
  bool GetSize(int64_t* size_out) final WARN_UNUSED_RESULT;
  bool IsStreaming() final;
  void SetBitrate(int bitrate) final;

 private:
  const std::string data_string_;
  const uint8_t* data_ = nullptr;
  const size_t size_ = 0;

  // Stop may be called from the render thread while this class is being used by
  // the media thread. It's harmless if we fulfill a read after Stop() has been
  // called, so an atomic without a lock is safe.
  std::atomic<bool> is_stopped_{false};
};

}  // namespace media

#endif  // MEDIA_FILTERS_MEMORY_DATA_SOURCE_H_
