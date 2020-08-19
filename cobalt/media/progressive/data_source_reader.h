// Copyright 2012 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_MEDIA_PROGRESSIVE_DATA_SOURCE_READER_H_
#define COBALT_MEDIA_PROGRESSIVE_DATA_SOURCE_READER_H_
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/media/base/data_source.h"

namespace cobalt {
namespace media {

// Allows sharing of a DataSource object between multiple objects on a single
// thread, and exposes a simple BlockingRead() method to block the thread until
// data is available or error. To avoid circular smart pointer references this
// object is also the sole owner of a pointer to DataSource.  If we want to add
// asynchronous reading to this object it will need its own thread and a
// callback queue.
class DataSourceReader : public base::RefCountedThreadSafe<DataSourceReader> {
 public:
  static const int kReadError;

  DataSourceReader();
  virtual void SetDataSource(DataSource* data_source);

  // Block the calling thread's message loop until read is complete.
  // returns number of bytes read or kReadError on error.
  // Currently only single-threaded support.
  virtual int BlockingRead(int64 position, int size, uint8* data);

  // returns size of file in bytes, or -1 if file size not known. If error will
  // retry getting file size on subsequent calls to FileSize().
  virtual int64 FileSize();

  // abort any pending read, then stop the data source
  virtual void Stop();

 protected:
  friend class base::RefCountedThreadSafe<DataSourceReader>;
  virtual ~DataSourceReader();
  // blocking read callback
  virtual void BlockingReadCompleted(int bytes_read);

  base::Lock lock_;
  DataSource* data_source_;
  base::WaitableEvent blocking_read_event_;
  int64 file_size_;
  bool read_has_failed_;
  int last_bytes_read_;  // protected implicitly by blocking_read_event_
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PROGRESSIVE_DATA_SOURCE_READER_H_
