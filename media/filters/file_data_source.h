// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FILE_DATA_SOURCE_H_
#define MEDIA_FILTERS_FILE_DATA_SOURCE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "media/base/data_source.h"

namespace media {

// Basic data source that treats the URL as a file path, and uses the file
// system to read data for a media pipeline.
class MEDIA_EXPORT FileDataSource : public DataSource {
 public:
  FileDataSource();
  FileDataSource(bool disable_file_size);

  PipelineStatus Initialize(const std::string& url);

  // Implementation of DataSource.
  virtual void set_host(DataSourceHost* host) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Read(int64 position, int size, uint8* data,
                    const DataSource::ReadCB& read_cb) OVERRIDE;
  virtual bool GetSize(int64* size_out) OVERRIDE;
  virtual bool IsStreaming() OVERRIDE;
  virtual void SetBitrate(int bitrate) OVERRIDE;

 protected:
  virtual ~FileDataSource();

 private:
  // Informs the host of changes in total and buffered bytes.
  void UpdateHostBytes();

  // File handle.  NULL if not initialized or an error occurs.
  FILE* file_;

  // Size of the file in bytes.
  int64 file_size_;

  // True if the FileDataSource should ignore its set file size, false
  // otherwise.
  bool disable_file_size_;

  // Critical section that protects all of the DataSource methods to prevent
  // a Stop from happening while in the middle of a file I/O operation.
  // TODO(ralphl): Ideally this would use asynchronous I/O or we will know
  // that we will block for a short period of time in reads.  Otherwise, we can
  // hang the pipeline Stop.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FILE_DATA_SOURCE_H_
