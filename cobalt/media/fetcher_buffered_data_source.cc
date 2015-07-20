/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/media/fetcher_buffered_data_source.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/file_path.h"
#include "base/path_service.h"

namespace cobalt {
namespace media {

FetcherBufferedDataSource::FetcherBufferedDataSource(
    MessageLoop* message_loop, const GURL& url,
    loader::FetcherFactory* fetcher_factory)
    : message_loop_(message_loop) {
  DCHECK(message_loop);
  DCHECK(fetcher_factory);
  DCHECK(!url.path().empty());

  state_ = kReading;
  fetcher_ = fetcher_factory->CreateFetcher(url, this);
  DCHECK(fetcher_);
  if (!fetcher_) {
    state_ = kError;
  }
}

FetcherBufferedDataSource::~FetcherBufferedDataSource() { fetcher_.reset(); }

void FetcherBufferedDataSource::Read(int64 position, int size, uint8* data,
                                     const ReadCB& read_cb) {
  DCHECK_GE(position, 0);
  DCHECK_GE(size, 0);

  base::AutoLock auto_lock(lock_);
  DCHECK(pending_read_cb_.is_null());
  if (state_ == kError) {
    message_loop_->PostTask(FROM_HERE, base::Bind(read_cb, -1));
    return;
  }

  int64 buffer_size = static_cast<int64>(buffer_.size());
  DCHECK_GE(buffer_size, 0);

  if (state_ == kFinishedReading) {
    position = std::min(position, buffer_size);
    size = static_cast<int>(std::min<int64>(size, buffer_size - position));
  }

  if (position + size <= buffer_size) {
    memcpy(data, &buffer_[static_cast<size_t>(position)],
           static_cast<size_t>(size));
    message_loop_->PostTask(FROM_HERE, base::Bind(read_cb, size));
    return;
  }

  pending_read_cb_ = read_cb;
  pending_read_position_ = position;
  pending_read_size_ = size;
  pending_read_data_ = data;
}

void FetcherBufferedDataSource::Stop(const base::Closure& callback) {
  {
    base::AutoLock auto_lock(lock_);

    if (!pending_read_cb_.is_null()) {
      base::ResetAndReturn(&pending_read_cb_).Run(0);
    }
  }

  // We cannot post the callback using the MessageLoop as we share the
  // same MessageLoop as WebMediaPlayerImpl (WMPI) and WMPI::Destroy()
  // waits on an event during video Stop.  The posted task will never be
  // run and will cause dead lock.
  callback.Run();
}

bool FetcherBufferedDataSource::GetSize(int64* size_out) {
  if (state_ == kFinishedReading) {
    // No need to acquire the lock as when state_ is kFinishedReading the
    // fetcher_ will no longer call any callbacks.
    *size_out = static_cast<int64>(buffer_.size());
    DCHECK_GE(*size_out, 0);
    return true;
  }
  return false;
}

void FetcherBufferedDataSource::OnReceived(const char* data, size_t size) {
  DCHECK_EQ(state_, kReading);
  if (size == 0) {
    return;
  }

  base::AutoLock auto_lock(lock_);
  buffer_.resize(buffer_.size() + size);
  memcpy(&buffer_[0] + buffer_.size() - size, data, size);
  ProcessPendingRead_Locked();
}

void FetcherBufferedDataSource::OnDone() {
  DCHECK_EQ(state_, kReading);
  base::AutoLock auto_lock(lock_);
  state_ = kFinishedReading;
  ProcessPendingRead_Locked();
}

void FetcherBufferedDataSource::OnError(const std::string& error) {
  DLOG(ERROR) << "FetcherBufferedDataSource::OnError() called with error "
              << error;

  DCHECK_EQ(state_, kReading);
  base::AutoLock auto_lock(lock_);
  state_ = kError;
  buffer_.clear();
  ProcessPendingRead_Locked();
}

void FetcherBufferedDataSource::ProcessPendingRead_Locked() {
  lock_.AssertAcquired();
  if (!pending_read_cb_.is_null()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&FetcherBufferedDataSource::Read, this,
                              pending_read_position_, pending_read_size_,
                              pending_read_data_,
                              base::ResetAndReturn(&pending_read_cb_)));
  }
}

}  // namespace media
}  // namespace cobalt
