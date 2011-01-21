// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FILE_DATA_SOURCE_H_
#define MEDIA_FILTERS_FILE_DATA_SOURCE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "media/base/filters.h"

namespace media {

// Basic data source that treats the URL as a file path, and uses the file
// system to read data for a media pipeline.
class FileDataSource : public DataSource {
 public:
  FileDataSource();
  virtual ~FileDataSource();

  // Implementation of Filter.
  virtual void Stop(FilterCallback* callback);

  // Implementation of DataSource.
  virtual void Initialize(const std::string& url, FilterCallback* callback);
  virtual const MediaFormat& media_format();
  virtual void Read(int64 position, size_t size, uint8* data,
                    ReadCallback* read_callback);
  virtual bool GetSize(int64* size_out);
  virtual bool IsStreaming();

 private:
  // Only allow factories and tests to create this object.
  //
  // TODO(scherkus): I'm getting tired of these factories getting in the way
  // of my tests!!!
  FRIEND_TEST_ALL_PREFIXES(FileDataSourceTest, OpenFile);
  FRIEND_TEST_ALL_PREFIXES(FileDataSourceTest, ReadData);
  FRIEND_TEST_ALL_PREFIXES(FileDataSourceTest, Seek);

  // File handle.  NULL if not initialized or an error occurs.
  FILE* file_;

  // Size of the file in bytes.
  int64 file_size_;

  // Media format handed out by the DataSource::GetMediaFormat method.
  MediaFormat media_format_;

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
