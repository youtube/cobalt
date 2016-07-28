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
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/string_number_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

namespace cobalt {
namespace media {

FetcherBufferedDataSource::FetcherBufferedDataSource(
    const scoped_refptr<base::MessageLoopProxy>& message_loop, const GURL& url,
    const csp::SecurityCallback& security_callback,
    network::NetworkModule* network_module)
    : message_loop_(message_loop),
      url_(url),
      network_module_(network_module),
      buffer_(kBufferCapacity, base::CircularBufferShell::kReserve),
      buffer_offset_(0),
      error_occured_(false),
      last_request_offset_(0),
      last_request_size_(0),
      last_read_position_(0),
      pending_read_position_(0),
      pending_read_size_(0),
      pending_read_data_(NULL),
      security_callback_(security_callback) {
  DCHECK(message_loop_);
  DCHECK(!url.path().empty());
  DCHECK(network_module);
}

FetcherBufferedDataSource::~FetcherBufferedDataSource() { DCHECK(!fetcher_); }

void FetcherBufferedDataSource::Read(int64 position, int size, uint8* data,
                                     const ReadCB& read_cb) {
  DCHECK_GE(position, 0);
  DCHECK_GE(size, 0);

  base::AutoLock auto_lock(lock_);
  Read_Locked(static_cast<uint64>(position), static_cast<uint64>(size), data,
              read_cb);
}

void FetcherBufferedDataSource::Stop(const base::Closure& callback) {
  {
    base::AutoLock auto_lock(lock_);

    if (!pending_read_cb_.is_null()) {
      base::ResetAndReturn(&pending_read_cb_).Run(0);
    }
    // From this moment on, any call to Read() should be treated as an error.
    error_occured_ = true;
    DestroyFetcher(fetcher_.Pass());
  }

  // We cannot post the callback using the MessageLoop as we share the
  // same MessageLoop as WebMediaPlayerImpl (WMPI) and WMPI::Destroy()
  // waits on an event during video Stop.  The posted task will never be
  // run and will cause dead lock.
  callback.Run();
}

bool FetcherBufferedDataSource::GetSize(int64* size_out) {
  base::AutoLock auto_lock(lock_);

  if (total_size_of_resource_) {
    *size_out = static_cast<int64>(total_size_of_resource_.value());
    DCHECK_GE(*size_out, 0);
  } else {
    *size_out = kInvalidSize;
  }
  return *size_out != kInvalidSize;
}

void FetcherBufferedDataSource::OnURLFetchResponseStarted(
    const net::URLFetcher* source) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(fetcher_.get(), source);
  if (fetcher_.get() != source) {
    return;
  }
  if (error_occured_) {
    return;
  }
  if (!source->GetStatus().is_success()) {
    // The error will be handled on OnURLFetchComplete()
    error_occured_ = true;
    return;
  } else if (source->GetResponseCode() == -1) {
    // Could be a file URL, so we won't expect headers.
    return;
  }

  // In the event of a redirect, re-check the security policy.
  if (source->GetURL() != source->GetOriginalURL()) {
    if (!security_callback_.is_null() &&
        !security_callback_.Run(source->GetURL(), true /*did redirect*/)) {
      error_occured_ = true;
      if (!pending_read_cb_.is_null()) {
        base::ResetAndReturn(&pending_read_cb_).Run(-1);
      }
      return;
    }
  }

  scoped_refptr<net::HttpResponseHeaders> headers =
      source->GetResponseHeaders();
  DCHECK(headers);

  uint64 first_byte_offset = 0;

  if (headers->response_code() == net::HTTP_PARTIAL_CONTENT) {
    int64 first_byte_position = -1;
    int64 last_byte_position = -1;
    int64 instance_length = -1;
    bool is_range_valid =
        headers &&
        headers->GetContentRange(&first_byte_position, &last_byte_position,
                                 &instance_length);
    if (is_range_valid) {
      if (first_byte_position >= 0) {
        first_byte_offset = static_cast<uint64>(first_byte_position);
      }
      if (!total_size_of_resource_ && instance_length > 0) {
        total_size_of_resource_ = static_cast<uint64>(instance_length);
      }
    }
  }

  DCHECK_LE(first_byte_offset, last_request_offset_);

  if (first_byte_offset < last_request_offset_) {
    last_request_size_ += last_request_offset_ - first_byte_offset;
    last_request_offset_ = first_byte_offset;
  }
}

void FetcherBufferedDataSource::OnURLFetchDownloadData(
    const net::URLFetcher* source, scoped_ptr<std::string> download_data) {
  UNREFERENCED_PARAMETER(source);
  DCHECK(message_loop_->BelongsToCurrentThread());
  size_t size = download_data->size();
  if (size == 0) {
    return;
  }
  const uint8* data = reinterpret_cast<const uint8*>(download_data->data());
  base::AutoLock auto_lock(lock_);
  if (fetcher_.get() != source) {
    return;
  }
  if (error_occured_) {
    return;
  }

  size = static_cast<size_t>(std::min<uint64>(size, last_request_size_));

  if (size == 0) {
    // The server side doesn't support range request.  Delete the fetcher to
    // stop the current request.
    DestroyFetcher(fetcher_.Pass());
    ProcessPendingRead_Locked();
    return;
  }

  // Because we can only append data into the buffer_.  We just check if the
  // position of the first byte of the newly received data is overlapped with
  // the range of the buffer_.  If not, we can discard all data in the buffer_
  // as there is no way to represent a gap or to prepend data.
  if (last_request_offset_ < buffer_offset_ ||
      last_request_offset_ > buffer_offset_ + buffer_.GetLength()) {
    buffer_.Clear();
    buffer_offset_ = last_request_offset_;
  }

  // If there is any overlapping, modify data/size accordingly.
  if (buffer_offset_ + buffer_.GetLength() > last_request_offset_) {
    uint64 difference =
        buffer_offset_ + buffer_.GetLength() - last_request_offset_;
    difference = std::min<uint64>(difference, size);
    data += difference;
    size -= difference;
    last_request_offset_ += difference;
  }

  // If we are overflow, remove some data from the front of the buffer_.
  if (buffer_.GetLength() + size > kBufferCapacity) {
    size_t bytes_skipped;
    buffer_.Skip(buffer_.GetLength() + size - kBufferCapacity, &bytes_skipped);
    // "+ 0" converts kBufferCapacity into a r-value to avoid link error.
    DCHECK_EQ(buffer_.GetLength() + size, kBufferCapacity + 0);
    buffer_offset_ += bytes_skipped;
  }

  size_t bytes_written;
  bool result = buffer_.Write(data, size, &bytes_written);
  DCHECK(result);
  DCHECK_EQ(size, bytes_written);

  last_request_offset_ += bytes_written;
  last_request_size_ -= bytes_written;

  ProcessPendingRead_Locked();
}

void FetcherBufferedDataSource::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (fetcher_.get() != source) {
    return;
  }
  const net::URLRequestStatus& status = source->GetStatus();
  if (status.is_success()) {
    if (total_size_of_resource_ && last_request_size_ != 0) {
      total_size_of_resource_ = buffer_offset_ + buffer_.GetLength();
    }
  } else {
    LOG(ERROR)
        << "FetcherBufferedDataSource::OnURLFetchComplete called with error "
        << status.error();
    error_occured_ = true;
    buffer_.Clear();
  }

  fetcher_.reset();

  ProcessPendingRead_Locked();
}

void FetcherBufferedDataSource::DestroyFetcher(
    scoped_ptr<net::URLFetcher> fetcher) {
  if (fetcher && !message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&FetcherBufferedDataSource::DestroyFetcher, this,
                              base::Passed(&fetcher)));
    return;
  }
  // Do nothing as |fetcher| will destroy itself.
}

void FetcherBufferedDataSource::CreateNewFetcher() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  DCHECK_GE(static_cast<int64>(last_request_offset_), 0);
  DCHECK_GE(static_cast<int64>(last_request_size_), 0);

  // Check if there was an error or if the request is blocked by csp.
  if (error_occured_ ||
      (!security_callback_.is_null() && !security_callback_.Run(url_, false))) {
    error_occured_ = true;
    if (!pending_read_cb_.is_null()) {
      base::ResetAndReturn(&pending_read_cb_).Run(-1);
    }
    return;
  }

  fetcher_.reset(net::URLFetcher::Create(url_, net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(network_module_->url_request_context_getter());
  fetcher_->DiscardResponse();

  std::string range_request =
      "Range: bytes=" + base::Uint64ToString(last_request_offset_) + "-" +
      base::Uint64ToString(last_request_offset_ + last_request_size_ - 1);
  fetcher_->AddExtraRequestHeader(range_request);
  fetcher_->Start();
}

void FetcherBufferedDataSource::Read_Locked(uint64 position, uint64 size,
                                            uint8* data,
                                            const ReadCB& read_cb) {
  lock_.AssertAcquired();

  DCHECK(data);
  DCHECK(!read_cb.is_null());
  DCHECK(pending_read_cb_.is_null());  // One read operation at the same time.

  if (error_occured_) {
    read_cb.Run(-1);
    return;
  }

  // Clamp the request to valid range of the resource if its size is known.
  if (total_size_of_resource_) {
    position = std::min(position, total_size_of_resource_.value());
    if (size + position > total_size_of_resource_.value()) {
      size = total_size_of_resource_.value() - position;
    }
  }

  last_read_position_ = position;

  if (size == 0) {
    read_cb.Run(0);
    return;
  }

  // Fulfill the read request now if we have the data.
  if (position >= buffer_offset_ &&
      position + size <= buffer_offset_ + buffer_.GetLength()) {
    // All data is available
    size_t bytes_peeked;
    buffer_.Peek(data, size, position - buffer_offset_, &bytes_peeked);
    DCHECK_EQ(bytes_peeked, size);
    DCHECK_GE(static_cast<int>(bytes_peeked), 0);
    read_cb.Run(static_cast<int>(bytes_peeked));
    // If we have a large buffer size, it could be ideal if we can keep sending
    // small requests when the read offset is far from the beginning of the
    // buffer.  However as the ShellDemuxer will cache many frames and the
    // buffer we are using is usually small, we will just avoid sending requests
    // here to make code simple.
    return;
  }

  // Save the read request as we are unable to fulfill it now.
  pending_read_cb_ = read_cb;
  pending_read_position_ = position;
  pending_read_size_ = size;
  pending_read_data_ = data;

  // Combine the range of the buffer and any ongoing fetch to see if the read
  // is overlapped with it.
  if (fetcher_) {
    uint64 begin = last_request_offset_;
    uint64 end = last_request_offset_ + last_request_size_;
    if (last_request_offset_ >= buffer_offset_ &&
        last_request_offset_ <= buffer_offset_ + buffer_.GetLength()) {
      begin = buffer_offset_;
    }
    if (position >= begin && position < end) {
      // The read is overlapped with existing request, just wait.
      return;
    }
  }

  // Now we have to issue a new fetch and we no longer care about the range
  // of the current fetch in progress if there is any.  Ideally the request
  // range starts at |last_read_position_ - kBackwardBytes| with length of
  // kBufferCapacity.
  if (last_read_position_ > kBackwardBytes) {
    last_request_offset_ = last_read_position_ - kBackwardBytes;
  } else {
    last_request_offset_ = 0;
  }

  last_request_size_ = kBufferCapacity;

  if (last_request_offset_ >= buffer_offset_ &&
      last_request_offset_ <= buffer_offset_ + buffer_.GetLength()) {
    // Part of the Read() can be fulfilled by the current buffer and current
    // request but cannot be fulfilled by the current request but we have to
    // send another request to retrieve the rest.
    last_request_size_ -=
        buffer_offset_ + buffer_.GetLength() - last_request_offset_;
    last_request_offset_ = buffer_offset_ + buffer_.GetLength();
  }

  DestroyFetcher(fetcher_.Pass());
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FetcherBufferedDataSource::CreateNewFetcher, this));
}

void FetcherBufferedDataSource::ProcessPendingRead_Locked() {
  lock_.AssertAcquired();
  if (!pending_read_cb_.is_null()) {
    Read_Locked(pending_read_position_, pending_read_size_, pending_read_data_,
                base::ResetAndReturn(&pending_read_cb_));
  }
}

}  // namespace media
}  // namespace cobalt
