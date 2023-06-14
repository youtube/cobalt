// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/url_fetcher_data_source.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/cors_preflight.h"
#include "cobalt/loader/url_fetcher_string_writer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

namespace cobalt {
namespace media {

namespace {

const uint32 kBackwardBytes = 256 * 1024;
const uint32 kInitialForwardBytes = 3 * 256 * 1024;
const uint32 kInitialBufferCapacity = kBackwardBytes + kInitialForwardBytes;

}  // namespace

using base::CircularBufferShell;

URLFetcherDataSource::URLFetcherDataSource(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const GURL& url, const csp::SecurityCallback& security_callback,
    network::NetworkModule* network_module, loader::RequestMode request_mode,
    loader::Origin origin)
    : task_runner_(task_runner),
      url_(url),
      network_module_(network_module),
      is_downloading_(false),
      buffer_(kInitialBufferCapacity, CircularBufferShell::kReserve),
      buffer_offset_(0),
      error_occured_(false),
      last_request_offset_(0),
      last_request_size_(0),
      last_read_position_(0),
      pending_read_position_(0),
      pending_read_size_(0),
      pending_read_data_(NULL),
      security_callback_(security_callback),
      request_mode_(request_mode),
      document_origin_(origin),
      is_origin_safe_(false) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(task_runner_);
  DCHECK(network_module);
}

URLFetcherDataSource::~URLFetcherDataSource() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (cancelable_create_fetcher_closure_) {
    cancelable_create_fetcher_closure_->Cancel();
  }
}

void URLFetcherDataSource::Read(int64 position, int size, uint8* data,
                                const ReadCB& read_cb) {
  DCHECK_GE(position, 0);
  DCHECK_GE(size, 0);

  if (position < 0 || size < 0) {
    read_cb.Run(kInvalidSize);
    return;
  }

  base::AutoLock auto_lock(lock_);
  Read_Locked(static_cast<uint64>(position), static_cast<size_t>(size), data,
              read_cb);
}

void URLFetcherDataSource::Stop() {
  {
    base::AutoLock auto_lock(lock_);

    if (!pending_read_cb_.is_null()) {
      base::ResetAndReturn(&pending_read_cb_).Run(0);
    }
    // From this moment on, any call to Read() should be treated as an error.
    // Note that we cannot reset |fetcher_| here because of:
    // 1. URLFetcher has to be destroyed on the thread that it is created,
    //    however Stop() is usually called from the pipeline thread where
    //    |fetcher_| is created on the web thread.
    // 2. We cannot post a task to the web thread to destroy |fetcher_| as the
    //    web thread is blocked by WebMediaPlayerImpl::Destroy().
    // Once |error_occured_| is set to true, the URLFetcher callbacks return
    // immediately and it is safe to destroy |fetcher_| inside the dtor.
    error_occured_ = true;
  }
}

bool URLFetcherDataSource::GetSize(int64* size_out) {
  base::AutoLock auto_lock(lock_);

  if (total_size_of_resource_) {
    *size_out = static_cast<int64>(total_size_of_resource_.value());
    DCHECK_GE(*size_out, 0);
  } else {
    *size_out = kInvalidSize;
  }
  return *size_out != kInvalidSize;
}

void URLFetcherDataSource::SetDownloadingStatusCB(
    const DownloadingStatusCB& downloading_status_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!downloading_status_cb.is_null());
  DCHECK(downloading_status_cb_.is_null());
  downloading_status_cb_ = downloading_status_cb;
}

void URLFetcherDataSource::OnURLFetchResponseStarted(
    const net::URLFetcher* source) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  if (fetcher_.get() != source || error_occured_) {
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

  if (!is_origin_safe_) {
    if (loader::CORSPreflight::CORSCheck(
            *headers, document_origin_.SerializedOrigin(),
            request_mode_ == loader::kCORSModeIncludeCredentials)) {
      is_origin_safe_ = true;
    } else {
      error_occured_ = true;
      if (!pending_read_cb_.is_null()) {
        base::ResetAndReturn(&pending_read_cb_).Run(-1);
      }
      return;
    }
  }

  uint64 first_byte_offset = 0;

  if (headers->response_code() == net::HTTP_PARTIAL_CONTENT) {
    int64 first_byte_position = -1;
    int64 last_byte_position = -1;
    int64 instance_length = -1;
    bool is_range_valid = headers && headers->GetContentRangeFor206(
                                         &first_byte_position,
                                         &last_byte_position, &instance_length);
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

void URLFetcherDataSource::OnURLFetchDownloadProgress(
    const net::URLFetcher* source, int64_t /*current*/, int64_t /*total*/,
    int64_t /*current_network_bytes*/) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto* download_data_writer =
      base::polymorphic_downcast<loader::URLFetcherStringWriter*>(
          source->GetResponseWriter());
  std::string downloaded_data;
  download_data_writer->GetAndResetData(&downloaded_data);
  size_t size = downloaded_data.size();
  if (size == 0) {
    return;
  }
  const uint8* data = reinterpret_cast<const uint8*>(downloaded_data.data());
  base::AutoLock auto_lock(lock_);

  if (fetcher_.get() != source || error_occured_) {
    return;
  }

  size = static_cast<size_t>(std::min<uint64>(size, last_request_size_));

  if (size == 0 || size > buffer_.GetMaxCapacity()) {
    // The server side doesn't support range request.  Delete |fetcher_| to
    // stop the current request.
    LOG(ERROR)
        << "URLFetcherDataSource::OnURLFetchDownloadProgress: server "
        << "doesn't support range requests (e.g. Python SimpleHTTPServer). "
        << "Please use a server that supports range requests (e.g. Flask).";
    error_occured_ = true;
    fetcher_.reset();
    ProcessPendingRead_Locked();
    UpdateDownloadingStatus(/* is_downloading = */ false);
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
  if (buffer_.GetLength() + size > buffer_.GetMaxCapacity()) {
    size_t bytes_skipped;
    buffer_.Skip(buffer_.GetLength() + size - buffer_.GetMaxCapacity(),
                 &bytes_skipped);
    // "+ 0" converts buffer_.GetMaxCapacity() into a r-value to avoid link
    // error.
    DCHECK_EQ(buffer_.GetLength() + size, buffer_.GetMaxCapacity() + 0);
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

void URLFetcherDataSource::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  if (fetcher_.get() != source || error_occured_) {
    return;
  }

  const net::URLRequestStatus& status = source->GetStatus();
  if (status.is_success()) {
    if (!total_size_of_resource_ && last_request_size_ != 0) {
      total_size_of_resource_ = buffer_offset_ + buffer_.GetLength();
    }
  } else {
    LOG(ERROR) << "URLFetcherDataSource::OnURLFetchComplete called with error "
               << status.error();
    error_occured_ = true;
    buffer_.Clear();
  }

  fetcher_.reset();

  ProcessPendingRead_Locked();
  UpdateDownloadingStatus(/* is_downloading = */ false);
}

void URLFetcherDataSource::CreateNewFetcher() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);

  DCHECK(!fetcher_);
  fetcher_to_be_destroyed_.reset();

  DCHECK_GE(static_cast<int64>(last_request_offset_), 0);
  DCHECK_GE(static_cast<int64>(last_request_size_), 0);

  // Check if there was an error or if the request is blocked by csp.
  if (error_occured_ ||
      (!security_callback_.is_null() && !security_callback_.Run(url_, false))) {
    error_occured_ = true;
    if (!pending_read_cb_.is_null()) {
      base::ResetAndReturn(&pending_read_cb_).Run(-1);
    }
    UpdateDownloadingStatus(/* is_downloading = */ false);
    return;
  }

  fetcher_ =
      std::move(net::URLFetcher::Create(url_, net::URLFetcher::GET, this));
  fetcher_->SetRequestContext(
      network_module_->url_request_context_getter().get());
  network_module_->AddClientHintHeaders(*fetcher_, network::kCallTypeMedia);
  std::unique_ptr<loader::URLFetcherStringWriter> download_data_writer(
      new loader::URLFetcherStringWriter());
  fetcher_->SaveResponseWithWriter(std::move(download_data_writer));

  std::string range_request =
      "Range: bytes=" + base::NumberToString(last_request_offset_) + "-" +
      base::NumberToString(last_request_offset_ + last_request_size_ - 1);
  fetcher_->AddExtraRequestHeader(range_request);
  if (!is_origin_safe_) {
    if (request_mode_ != loader::kNoCORSMode &&
        document_origin_ != loader::Origin(url_) && !url_.SchemeIs("data")) {
      fetcher_->AddExtraRequestHeader("Origin:" +
                                      document_origin_.SerializedOrigin());
    } else {
      is_origin_safe_ = true;
    }
  }
  fetcher_->Start();
  UpdateDownloadingStatus(/* is_downloading = */ true);
}

void URLFetcherDataSource::UpdateDownloadingStatus(bool is_downloading) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (is_downloading_ == is_downloading) {
    return;
  }

  is_downloading_ = is_downloading;
  if (!downloading_status_cb_.is_null()) {
    downloading_status_cb_.Run(is_downloading_);
  }
}

void URLFetcherDataSource::Read_Locked(uint64 position, size_t size,
                                       uint8* data, const ReadCB& read_cb) {
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
      size = static_cast<size_t>(total_size_of_resource_.value() - position);
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
    buffer_.Peek(data, size, static_cast<size_t>(position - buffer_offset_),
                 &bytes_peeked);
    DCHECK_EQ(bytes_peeked, size);
    DCHECK_GE(static_cast<int>(bytes_peeked), 0);
    read_cb.Run(static_cast<int>(bytes_peeked));
    // If we have a large buffer size, it could be ideal if we can keep sending
    // small requests when the read offset is far from the beginning of the
    // buffer.  However as the ProgressiveDemuxer will cache many frames and the
    // buffer we are using is usually small, we will just avoid sending requests
    // here to make code simple.
    return;
  }

  // Save the read request as we are unable to fulfill it now.
  pending_read_cb_ = read_cb;
  pending_read_position_ = position;
  pending_read_size_ = size;
  pending_read_data_ = data;

  // Combine the range of the buffer and any ongoing fetch to see if the read is
  // overlapped with it.
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

  // Now we have to issue a new fetch and we no longer care about the range of
  // the current fetch in progress if there is any.  Ideally the request range
  // starts at |last_read_position_ - kBackwardBytes| with length of
  // buffer_.GetMaxCapacity().
  if (last_read_position_ > kBackwardBytes) {
    last_request_offset_ = last_read_position_ - kBackwardBytes;
  } else {
    last_request_offset_ = 0;
  }

  size_t required_size = static_cast<size_t>(
      last_read_position_ - last_request_offset_ + pending_read_size_);
  if (required_size > buffer_.GetMaxCapacity()) {
    // The capacity of the current buffer is not large enough to hold the
    // pending read.
    size_t new_capacity =
        std::max<size_t>(buffer_.GetMaxCapacity() * 2, required_size);
    buffer_.IncreaseMaxCapacityTo(new_capacity);
  }

  last_request_size_ = buffer_.GetMaxCapacity();

  if (last_request_offset_ >= buffer_offset_ &&
      last_request_offset_ <= buffer_offset_ + buffer_.GetLength()) {
    // Part of the Read() can be fulfilled by the current buffer and current
    // request but cannot be fulfilled by the current request but we have to
    // send another request to retrieve the rest.
    last_request_size_ -=
        buffer_offset_ + buffer_.GetLength() - last_request_offset_;
    last_request_offset_ = buffer_offset_ + buffer_.GetLength();
  }

  if (cancelable_create_fetcher_closure_) {
    cancelable_create_fetcher_closure_->Cancel();
  }
  base::Closure create_fetcher_closure = base::Bind(
      &URLFetcherDataSource::CreateNewFetcher, base::Unretained(this));
  cancelable_create_fetcher_closure_ =
      new CancelableClosure(create_fetcher_closure);
  fetcher_to_be_destroyed_.reset(fetcher_.release());
  task_runner_->PostTask(FROM_HERE,
                         cancelable_create_fetcher_closure_->AsClosure());
}

void URLFetcherDataSource::ProcessPendingRead_Locked() {
  lock_.AssertAcquired();
  if (!pending_read_cb_.is_null()) {
    Read_Locked(pending_read_position_, pending_read_size_, pending_read_data_,
                base::ResetAndReturn(&pending_read_cb_));
  }
}

URLFetcherDataSource::CancelableClosure::CancelableClosure(
    const base::Closure& closure)
    : closure_(closure) {
  DCHECK(!closure.is_null());
}

void URLFetcherDataSource::CancelableClosure::Cancel() {
  base::AutoLock auto_lock(lock_);
  closure_.Reset();
}

base::Closure URLFetcherDataSource::CancelableClosure::AsClosure() {
  return base::Bind(&CancelableClosure::Call, this);
}

void URLFetcherDataSource::CancelableClosure::Call() {
  base::AutoLock auto_lock(lock_);
  // closure_.Run() has to be called when the lock is acquired to avoid race
  // condition.
  if (!closure_.is_null()) {
    base::ResetAndReturn(&closure_).Run();
  }
}

}  // namespace media
}  // namespace cobalt
