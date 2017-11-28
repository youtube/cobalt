// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FILE_DATA_SOURCE_H_
#define MEDIA_FILTERS_FILE_DATA_SOURCE_H_

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "media/base/data_source.h"

namespace media {

// Basic data source that treats the URL as a file path, and uses the file
// system to read data for a media pipeline.
class MEDIA_EXPORT FileDataSource : public DataSource {
 public:
  FileDataSource();

  bool Initialize(const FilePath& file_path);

  // Implementation of DataSource.
  virtual void set_host(DataSourceHost* host) override;
  virtual void Stop(const base::Closure& callback) override;
  virtual void Read(int64 position, int size, uint8* data,
                    const DataSource::ReadCB& read_cb) override;
  virtual bool GetSize(int64* size_out) override;
  virtual bool IsStreaming() override;
  virtual void SetBitrate(int bitrate) override;

  // Unit test helpers. Recreate the object if you want the default behaviour.
  void force_read_errors_for_testing() { force_read_errors_ = true; }
  void force_streaming_for_testing() { force_streaming_ = true; }

 protected:
  virtual ~FileDataSource();

 private:
  // Informs the host of changes in total and buffered bytes.
  void UpdateHostBytes();

  file_util::MemoryMappedFile file_;

  bool force_read_errors_;
  bool force_streaming_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FILE_DATA_SOURCE_H_
