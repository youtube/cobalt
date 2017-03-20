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

#ifndef MEDIA_BASE_SHELL_DATA_SOURCE_READER_H_
#define MEDIA_BASE_SHELL_DATA_SOURCE_READER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/data_source.h"

namespace media {

// Allows sharing of a DataSource object between multiple objects on a single
// thread, and exposes a simple BlockingRead() method to block the thread until
// data is available or error. To avoid circular smart pointer references this
// object is also the sole owner of a pointer to DataSource.  If we want to add
// asynchronous reading to this object it will need its own thread and a
// callback queue.
class ShellDataSourceReader
    : public base::RefCountedThreadSafe<ShellDataSourceReader> {
 public:
  static const int kReadError;

  ShellDataSourceReader();
  virtual void SetDataSource(DataSource* data_source);

  // Block the calling thread's message loop until read is complete.
  // returns number of bytes read or kReadError on error.
  // Currently only single-threaded support.
  virtual int BlockingRead(int64 position, int size, uint8* data);

  // returns size of file in bytes, or -1 if file size not known. If error will
  // retry getting file size on subsequent calls to FileSize().
  virtual int64 FileSize();

  // abort any pending read, then stop the data source
  virtual void Stop(const base::Closure& callback);

 protected:
  friend class base::RefCountedThreadSafe<ShellDataSourceReader>;
  virtual ~ShellDataSourceReader();
  // blocking read callback
  virtual void BlockingReadCompleted(int bytes_read);

  DataSource* data_source_;
  base::WaitableEvent blocking_read_event_;
  int64 file_size_;
  bool read_has_failed_;
  int last_bytes_read_;  // protected implicitly by blocking_read_event_
};

}  // namespace media

#endif  // MEDIA_BASE_SHELL_DATA_SOURCE_READER_H_
