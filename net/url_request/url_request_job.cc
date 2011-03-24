// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_tracker.h"

namespace net {

URLRequestJob::URLRequestJob(URLRequest* request)
    : request_(request),
      done_(false),
      prefilter_bytes_read_(0),
      postfilter_bytes_read_(0),
      filter_input_byte_count_(0),
      filter_needs_more_output_space_(false),
      filtered_read_buffer_len_(0),
      has_handled_response_(false),
      expected_content_size_(-1),
      deferred_redirect_status_code_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  g_url_request_job_tracker.AddNewJob(this);
}

void URLRequestJob::SetUpload(UploadData* upload) {
}

void URLRequestJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
}

void URLRequestJob::Kill() {
  // Make sure the request is notified that we are done.  We assume that the
  // request took care of setting its error status before calling Kill.
  if (request_)
    NotifyCanceled();
}

void URLRequestJob::DetachRequest() {
  request_ = NULL;
}

// This function calls ReadData to get stream data. If a filter exists, passes
// the data to the attached filter. Then returns the output from filter back to
// the caller.
bool URLRequestJob::Read(IOBuffer* buf, int buf_size, int *bytes_read) {
  bool rv = false;

  DCHECK_LT(buf_size, 1000000);  // sanity check
  DCHECK(buf);
  DCHECK(bytes_read);
  DCHECK(filtered_read_buffer_ == NULL);
  DCHECK_EQ(0, filtered_read_buffer_len_);

  *bytes_read = 0;

  // Skip Filter if not present
  if (!filter_.get()) {
    rv = ReadRawDataHelper(buf, buf_size, bytes_read);
  } else {
    // Save the caller's buffers while we do IO
    // in the filter's buffers.
    filtered_read_buffer_ = buf;
    filtered_read_buffer_len_ = buf_size;

    if (ReadFilteredData(bytes_read)) {
      rv = true;   // we have data to return
    } else {
      rv = false;  // error, or a new IO is pending
    }
  }
  if (rv && *bytes_read == 0)
    NotifyDone(URLRequestStatus());
  return rv;
}

void URLRequestJob::StopCaching() {
  // Nothing to do here.
}

LoadState URLRequestJob::GetLoadState() const {
  return LOAD_STATE_IDLE;
}

uint64 URLRequestJob::GetUploadProgress() const {
  return 0;
}

bool URLRequestJob::GetCharset(std::string* charset) {
  return false;
}

void URLRequestJob::GetResponseInfo(HttpResponseInfo* info) {
}

bool URLRequestJob::GetResponseCookies(std::vector<std::string>* cookies) {
  return false;
}

Filter* URLRequestJob::SetupFilter() const {
  return NULL;
}

bool URLRequestJob::IsRedirectResponse(GURL* location,
                                       int* http_status_code) {
  // For non-HTTP jobs, headers will be null.
  HttpResponseHeaders* headers = request_->response_headers();
  if (!headers)
    return false;

  std::string value;
  if (!headers->IsRedirect(&value))
    return false;

  *location = request_->url().Resolve(value);
  *http_status_code = headers->response_code();
  return true;
}

bool URLRequestJob::IsSafeRedirect(const GURL& location) {
  return true;
}

bool URLRequestJob::NeedsAuth() {
  return false;
}

void URLRequestJob::GetAuthChallengeInfo(
    scoped_refptr<AuthChallengeInfo>* auth_info) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::SetAuth(const string16& username,
                            const string16& password) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::CancelAuth() {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::ContinueWithCertificate(
    X509Certificate* client_cert) {
  // The derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::ContinueDespiteLastError() {
  // Implementations should know how to recover from errors they generate.
  // If this code was reached, we are trying to recover from an error that
  // we don't know how to recover from.
  NOTREACHED();
}

void URLRequestJob::FollowDeferredRedirect() {
  DCHECK(deferred_redirect_status_code_ != -1);

  // NOTE: deferred_redirect_url_ may be invalid, and attempting to redirect to
  // such an URL will fail inside FollowRedirect.  The DCHECK above asserts
  // that we called OnReceivedRedirect.

  // It is also possible that FollowRedirect will drop the last reference to
  // this job, so we need to reset our members before calling it.

  GURL redirect_url = deferred_redirect_url_;
  int redirect_status_code = deferred_redirect_status_code_;

  deferred_redirect_url_ = GURL();
  deferred_redirect_status_code_ = -1;

  FollowRedirect(redirect_url, redirect_status_code);
}

bool URLRequestJob::GetMimeType(std::string* mime_type) const {
  return false;
}

int URLRequestJob::GetResponseCode() const {
  return -1;
}

HostPortPair URLRequestJob::GetSocketAddress() const {
  return HostPortPair();
}

URLRequestJob::~URLRequestJob() {
  g_url_request_job_tracker.RemoveJob(this);
}

void URLRequestJob::NotifyHeadersComplete() {
  if (!request_ || !request_->delegate())
    return;  // The request was destroyed, so there is no more work to do.

  if (has_handled_response_)
    return;

  DCHECK(!request_->status().is_io_pending());

  // Initialize to the current time, and let the subclass optionally override
  // the time stamps if it has that information.  The default request_time is
  // set by URLRequest before it calls our Start method.
  request_->response_info_.response_time = base::Time::Now();
  GetResponseInfo(&request_->response_info_);

  // When notifying the delegate, the delegate can release the request
  // (and thus release 'this').  After calling to the delgate, we must
  // check the request pointer to see if it still exists, and return
  // immediately if it has been destroyed.  self_preservation ensures our
  // survival until we can get out of this method.
  scoped_refptr<URLRequestJob> self_preservation(this);

  GURL new_location;
  int http_status_code;
  if (IsRedirectResponse(&new_location, &http_status_code)) {
    const GURL& url = request_->url();

    // Move the reference fragment of the old location to the new one if the
    // new one has none. This duplicates mozilla's behavior.
    if (url.is_valid() && url.has_ref() && !new_location.has_ref()) {
      GURL::Replacements replacements;
      // Reference the |ref| directly out of the original URL to avoid a
      // malloc.
      replacements.SetRef(url.spec().data(),
                          url.parsed_for_possibly_invalid_spec().ref);
      new_location = new_location.ReplaceComponents(replacements);
    }

    bool defer_redirect = false;
    request_->ReceivedRedirect(new_location, &defer_redirect);

    // Ensure that the request wasn't detached or destroyed in ReceivedRedirect
    if (!request_ || !request_->delegate())
      return;

    // If we were not cancelled, then maybe follow the redirect.
    if (request_->status().is_success()) {
      if (defer_redirect) {
        deferred_redirect_url_ = new_location;
        deferred_redirect_status_code_ = http_status_code;
      } else {
        FollowRedirect(new_location, http_status_code);
      }
      return;
    }
  } else if (NeedsAuth()) {
    scoped_refptr<AuthChallengeInfo> auth_info;
    GetAuthChallengeInfo(&auth_info);
    // Need to check for a NULL auth_info because the server may have failed
    // to send a challenge with the 401 response.
    if (auth_info) {
      request_->delegate()->OnAuthRequired(request_, auth_info);
      // Wait for SetAuth or CancelAuth to be called.
      return;
    }
  }

  has_handled_response_ = true;
  if (request_->status().is_success())
    filter_.reset(SetupFilter());

  if (!filter_.get()) {
    std::string content_length;
    request_->GetResponseHeaderByName("content-length", &content_length);
    if (!content_length.empty())
      base::StringToInt64(content_length, &expected_content_size_);
  }

  request_->ResponseStarted();
}

void URLRequestJob::NotifyReadComplete(int bytes_read) {
  if (!request_ || !request_->delegate())
    return;  // The request was destroyed, so there is no more work to do.

  // TODO(darin): Bug 1004233. Re-enable this test once all of the chrome
  // unit_tests have been fixed to not trip this.
  //DCHECK(!request_->status().is_io_pending());

  // The headers should be complete before reads complete
  DCHECK(has_handled_response_);

  OnRawReadComplete(bytes_read);

  // Don't notify if we had an error.
  if (!request_->status().is_success())
    return;

  // When notifying the delegate, the delegate can release the request
  // (and thus release 'this').  After calling to the delgate, we must
  // check the request pointer to see if it still exists, and return
  // immediately if it has been destroyed.  self_preservation ensures our
  // survival until we can get out of this method.
  scoped_refptr<URLRequestJob> self_preservation(this);

  prefilter_bytes_read_ += bytes_read;
  if (filter_.get()) {
    // Tell the filter that it has more data
    FilteredDataRead(bytes_read);

    // Filter the data.
    int filter_bytes_read = 0;
    if (ReadFilteredData(&filter_bytes_read)) {
      postfilter_bytes_read_ += filter_bytes_read;
      if (request_->context() && request_->context()->network_delegate()) {
        request_->context()->network_delegate()->NotifyReadCompleted(
            request_, filter_bytes_read);
      }
      request_->delegate()->OnReadCompleted(request_, filter_bytes_read);
    }
  } else {
    postfilter_bytes_read_ += bytes_read;
    if (request_->context() && request_->context()->network_delegate()) {
      request_->context()->network_delegate()->NotifyReadCompleted(
          request_, bytes_read);
    }
    request_->delegate()->OnReadCompleted(request_, bytes_read);
  }
}

void URLRequestJob::NotifyStartError(const URLRequestStatus &status) {
  DCHECK(!has_handled_response_);
  has_handled_response_ = true;
  if (request_) {
    request_->set_status(status);
    request_->ResponseStarted();
  }
}

void URLRequestJob::NotifyDone(const URLRequestStatus &status) {
  DCHECK(!done_) << "Job sending done notification twice";
  if (done_)
    return;
  done_ = true;

  // Unless there was an error, we should have at least tried to handle
  // the response before getting here.
  DCHECK(has_handled_response_ || !status.is_success());

  // As with NotifyReadComplete, we need to take care to notice if we were
  // destroyed during a delegate callback.
  if (request_) {
    request_->set_is_pending(false);
    // With async IO, it's quite possible to have a few outstanding
    // requests.  We could receive a request to Cancel, followed shortly
    // by a successful IO.  For tracking the status(), once there is
    // an error, we do not change the status back to success.  To
    // enforce this, only set the status if the job is so far
    // successful.
    if (request_->status().is_success())
      request_->set_status(status);
  }

  g_url_request_job_tracker.OnJobDone(this, status);

  // Complete this notification later.  This prevents us from re-entering the
  // delegate if we're done because of a synchronous call.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&URLRequestJob::CompleteNotifyDone));
}

void URLRequestJob::CompleteNotifyDone() {
  // Check if we should notify the delegate that we're done because of an error.
  if (request_ &&
      !request_->status().is_success() &&
      request_->delegate()) {
    // We report the error differently depending on whether we've called
    // OnResponseStarted yet.
    if (has_handled_response_) {
      // We signal the error by calling OnReadComplete with a bytes_read of -1.
      if (request_->context() && request_->context()->network_delegate())
        request_->context()->network_delegate()->NotifyReadCompleted(
            request_, -1);
      request_->delegate()->OnReadCompleted(request_, -1);
    } else {
      has_handled_response_ = true;
      request_->ResponseStarted();
    }
  }
}

void URLRequestJob::NotifyCanceled() {
  if (!done_) {
    NotifyDone(URLRequestStatus(URLRequestStatus::CANCELED,
                                ERR_ABORTED));
  }
}

void URLRequestJob::NotifyRestartRequired() {
  DCHECK(!has_handled_response_);
  if (GetStatus().status() != URLRequestStatus::CANCELED)
    request_->Restart();
}

bool URLRequestJob::ReadRawData(IOBuffer* buf, int buf_size,
                                int *bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;
  NotifyDone(URLRequestStatus());
  return false;
}

void URLRequestJob::FilteredDataRead(int bytes_read) {
  DCHECK(filter_.get());  // don't add data if there is no filter
  filter_->FlushStreamBuffer(bytes_read);
}

bool URLRequestJob::ReadFilteredData(int* bytes_read) {
  DCHECK(filter_.get());  // don't add data if there is no filter
  DCHECK(filtered_read_buffer_ != NULL);  // we need to have a buffer to fill
  DCHECK_GT(filtered_read_buffer_len_, 0);  // sanity check
  DCHECK_LT(filtered_read_buffer_len_, 1000000);  // sanity check
  DCHECK(raw_read_buffer_ == NULL);  // there should be no raw read buffer yet

  bool rv = false;
  *bytes_read = 0;

  if (is_done())
    return true;

  if (!filter_needs_more_output_space_ && !filter_->stream_data_len()) {
    // We don't have any raw data to work with, so
    // read from the socket.
    int filtered_data_read;
    if (ReadRawDataForFilter(&filtered_data_read)) {
      if (filtered_data_read > 0) {
        filter_->FlushStreamBuffer(filtered_data_read);  // Give data to filter.
      } else {
        return true;  // EOF
      }
    } else {
      return false;  // IO Pending (or error)
    }
  }

  if ((filter_->stream_data_len() || filter_needs_more_output_space_)
      && !is_done()) {
    // Get filtered data.
    int filtered_data_len = filtered_read_buffer_len_;
    Filter::FilterStatus status;
    int output_buffer_size = filtered_data_len;
    status = filter_->ReadData(filtered_read_buffer_->data(),
                               &filtered_data_len);

    if (filter_needs_more_output_space_ && 0 == filtered_data_len) {
      // filter_needs_more_output_space_ was mistaken... there are no more bytes
      // and we should have at least tried to fill up the filter's input buffer.
      // Correct the state, and try again.
      filter_needs_more_output_space_ = false;
      return ReadFilteredData(bytes_read);
    }

    switch (status) {
      case Filter::FILTER_DONE: {
        filter_needs_more_output_space_ = false;
        *bytes_read = filtered_data_len;
        rv = true;
        break;
      }
      case Filter::FILTER_NEED_MORE_DATA: {
        filter_needs_more_output_space_ =
            (filtered_data_len == output_buffer_size);
        // We have finished filtering all data currently in the buffer.
        // There might be some space left in the output buffer. One can
        // consider reading more data from the stream to feed the filter
        // and filling up the output buffer. This leads to more complicated
        // buffer management and data notification mechanisms.
        // We can revisit this issue if there is a real perf need.
        if (filtered_data_len > 0) {
          *bytes_read = filtered_data_len;
          rv = true;
        } else {
          // Read again since we haven't received enough data yet (e.g., we may
          // not have a complete gzip header yet)
          rv = ReadFilteredData(bytes_read);
        }
        break;
      }
      case Filter::FILTER_OK: {
        filter_needs_more_output_space_ =
            (filtered_data_len == output_buffer_size);
        *bytes_read = filtered_data_len;
        rv = true;
        break;
      }
      case Filter::FILTER_ERROR: {
        filter_needs_more_output_space_ = false;
        NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                   ERR_CONTENT_DECODING_FAILED));
        rv = false;
        break;
      }
      default: {
        NOTREACHED();
        filter_needs_more_output_space_ = false;
        rv = false;
        break;
      }
    }
  } else {
    // we are done, or there is no data left.
    rv = true;
  }

  if (rv) {
    // When we successfully finished a read, we no longer need to
    // save the caller's buffers. Release our reference.
    filtered_read_buffer_ = NULL;
    filtered_read_buffer_len_ = 0;
  }
  return rv;
}

const URLRequestStatus URLRequestJob::GetStatus() {
  if (request_)
    return request_->status();
  // If the request is gone, we must be cancelled.
  return URLRequestStatus(URLRequestStatus::CANCELED,
                          ERR_ABORTED);
}

void URLRequestJob::SetStatus(const URLRequestStatus &status) {
  if (request_)
    request_->set_status(status);
}

bool URLRequestJob::ReadRawDataForFilter(int* bytes_read) {
  bool rv = false;

  DCHECK(bytes_read);
  DCHECK(filter_.get());

  *bytes_read = 0;

  // Get more pre-filtered data if needed.
  // TODO(mbelshe): is it possible that the filter needs *MORE* data
  //    when there is some data already in the buffer?
  if (!filter_->stream_data_len() && !is_done()) {
    IOBuffer* stream_buffer = filter_->stream_buffer();
    int stream_buffer_size = filter_->stream_buffer_size();
    rv = ReadRawDataHelper(stream_buffer, stream_buffer_size, bytes_read);
  }
  return rv;
}

bool URLRequestJob::ReadRawDataHelper(IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  DCHECK(!request_->status().is_io_pending());
  DCHECK(raw_read_buffer_ == NULL);

  // Keep a pointer to the read buffer, so we have access to it in the
  // OnRawReadComplete() callback in the event that the read completes
  // asynchronously.
  raw_read_buffer_ = buf;
  bool rv = ReadRawData(buf, buf_size, bytes_read);

  if (!request_->status().is_io_pending()) {
    // If the read completes synchronously, either success or failure,
    // invoke the OnRawReadComplete callback so we can account for the
    // completed read.
    OnRawReadComplete(*bytes_read);
  }
  return rv;
}

void URLRequestJob::FollowRedirect(const GURL& location, int http_status_code) {
  g_url_request_job_tracker.OnJobRedirect(this, location, http_status_code);

  int rv = request_->Redirect(location, http_status_code);
  if (rv != OK)
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

void URLRequestJob::OnRawReadComplete(int bytes_read) {
  DCHECK(raw_read_buffer_);
  if (bytes_read > 0) {
    RecordBytesRead(bytes_read);
  }
  raw_read_buffer_ = NULL;
}

void URLRequestJob::RecordBytesRead(int bytes_read) {
  filter_input_byte_count_ += bytes_read;
  UpdatePacketReadTimes();  // Facilitate stats recording if it is active.
  g_url_request_job_tracker.OnBytesRead(this, raw_read_buffer_->data(),
                                        bytes_read);
}

bool URLRequestJob::FilterHasData() {
    return filter_.get() && filter_->stream_data_len();
}

void URLRequestJob::UpdatePacketReadTimes() {
}

}  // namespace net
