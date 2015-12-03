/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/base/shell_data_source_reader.h"

#include <limits.h>  // for ULLONG_MAX

namespace media {

const int ShellDataSourceReader::kReadError = DataSource::kReadError;

ShellDataSourceReader::ShellDataSourceReader()
    : blocking_read_event_(false, false),
      file_size_(-1),
      read_has_failed_(false),
      last_bytes_read_(0) {}

ShellDataSourceReader::~ShellDataSourceReader() {}

void ShellDataSourceReader::SetDataSource(
    scoped_refptr<DataSource> data_source) {
  data_source_ = data_source;
}

// currently only single-threaded reads supported
int ShellDataSourceReader::BlockingRead(int64 position, int size, uint8* data) {
  // read failures are unrecoverable, all subsequent reads will also fail
  if (read_has_failed_) {
    return kReadError;
  }

  // check bounds of read at or past EOF
  if (file_size_ >= 0 && position >= file_size_) {
    return 0;
  }

  int total_bytes_read = 0;
  while (size > 0 && !read_has_failed_) {
    data_source_->Read(
        position, size, data,
        base::Bind(&ShellDataSourceReader::BlockingReadCompleted, this));

    // wait for callback on read completion
    blocking_read_event_.Wait();

    if (last_bytes_read_ == DataSource::kReadError) {
      // make all future reads fail
      read_has_failed_ = true;
      return kReadError;
    }

    DCHECK_LE(last_bytes_read_, size);
    if (last_bytes_read_ > size) {
      // make all future reads fail
      read_has_failed_ = true;
      return kReadError;
    }

    // Avoid entering an endless loop here.
    if (last_bytes_read_ == 0)
      break;

    total_bytes_read += last_bytes_read_;
    position += last_bytes_read_;
    size -= last_bytes_read_;
    data += last_bytes_read_;
  }

  if (read_has_failed_) {
    return kReadError;
  }
  return total_bytes_read;
}

void ShellDataSourceReader::Stop(const base::Closure& callback) {
  if (data_source_) {
    // stop the data source, it can call the callback
    data_source_->Stop(callback);
  }
}

void ShellDataSourceReader::BlockingReadCompleted(int bytes_read) {
  last_bytes_read_ = bytes_read;
  // wake up blocked thread
  blocking_read_event_.Signal();
}

int64 ShellDataSourceReader::FileSize() {
  if (file_size_ == -1) {
    if (!data_source_->GetSize(&file_size_)) {
      file_size_ = -1;
    }
  }
  return file_size_;
}

}  // namespace media
