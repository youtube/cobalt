// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_metrics.h"
#include "net/url_request/url_request_job_tracker.h"

using base::Time;
using base::TimeTicks;

namespace net {

URLRequestJob::URLRequestJob(URLRequest* request)
    : request_(request),
      prefilter_bytes_read_(0),
      postfilter_bytes_read_(0),
      is_compressible_content_(false),
      is_compressed_(false),
      done_(false),
      filter_needs_more_output_space_(false),
      filtered_read_buffer_len_(0),
      has_handled_response_(false),
      expected_content_size_(-1),
      deferred_redirect_status_code_(-1),
      packet_timing_enabled_(false),
      filter_input_byte_count_(0),
      bytes_observed_in_packets_(0),
      max_packets_timed_(0),
      observed_packet_count_(0) {
  load_flags_ = request_->load_flags();
  is_profiling_ = request->enable_profiling();
  if (is_profiling()) {
    metrics_.reset(new URLRequestJobMetrics());
    metrics_->start_time_ = TimeTicks::Now();
  }
  g_url_request_job_tracker.AddNewJob(this);
}

void URLRequestJob::SetUpload(net::UploadData* upload) {
}

void URLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
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
bool URLRequestJob::Read(net::IOBuffer* buf, int buf_size, int *bytes_read) {
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

net::LoadState URLRequestJob::GetLoadState() const {
  return net::LOAD_STATE_IDLE;
}

uint64 URLRequestJob::GetUploadProgress() const {
  return 0;
}

bool URLRequestJob::GetCharset(std::string* charset) {
  return false;
}

void URLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
}

bool URLRequestJob::GetResponseCookies(std::vector<std::string>* cookies) {
  return false;
}

bool URLRequestJob::GetContentEncodings(
    std::vector<Filter::FilterType>* encoding_types) {
  return false;
}

void URLRequestJob::SetupFilter() {
  std::vector<Filter::FilterType> encoding_types;
  if (GetContentEncodings(&encoding_types)) {
    filter_.reset(Filter::Factory(encoding_types, *this));
  }
}

bool URLRequestJob::IsRedirectResponse(GURL* location,
                                       int* http_status_code) {
  // For non-HTTP jobs, headers will be null.
  net::HttpResponseHeaders* headers = request_->response_headers();
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
    scoped_refptr<net::AuthChallengeInfo>* auth_info) {
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
    net::X509Certificate* client_cert) {
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

URLRequestJobMetrics* URLRequestJob::RetrieveMetrics() {
  if (is_profiling())
    return metrics_.release();
  else
    return NULL;
}

bool URLRequestJob::GetMimeType(std::string* mime_type) const {
  return false;
}

bool URLRequestJob::GetURL(GURL* gurl) const {
  if (!request_)
    return false;
  *gurl = request_->url();
  return true;
}

base::Time URLRequestJob::GetRequestTime() const {
  if (!request_)
    return base::Time();
  return request_->request_time();
}

bool URLRequestJob::IsDownload() const {
  return (load_flags_ & net::LOAD_IS_DOWNLOAD) != 0;
}

bool URLRequestJob::IsSdchResponse() const {
  return false;
}

bool URLRequestJob::IsCachedContent() const {
  return false;
}

int64 URLRequestJob::GetByteReadCount() const {
  return filter_input_byte_count_;
}

int URLRequestJob::GetResponseCode() const {
  return -1;
}

void URLRequestJob::RecordPacketStats(StatisticSelector statistic) const {
  if (!packet_timing_enabled_ || (final_packet_time_ == base::Time()))
    return;

  // Caller should verify that we're not cached content, but we can't always
  // really check for it here because we may (at destruction time) call our own
  // class method and get a bogus const answer of false. This DCHECK only helps
  // when this method has a valid overridden definition.
  DCHECK(!IsCachedContent());

  base::TimeDelta duration = final_packet_time_ - request_time_snapshot_;
  switch (statistic) {
    case SDCH_DECODE: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_Latency_F_a", duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_COUNTS_100("Sdch3.Network_Decode_Packets_b",
                               static_cast<int>(observed_packet_count_));
      UMA_HISTOGRAM_CUSTOM_COUNTS("Sdch3.Network_Decode_Bytes_Processed_b",
          static_cast<int>(bytes_observed_in_packets_), 500, 100000, 100);
      if (packet_times_.empty())
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_1st_To_Last_a",
                                  final_packet_time_ - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);

      DCHECK(max_packets_timed_ >= kSdchPacketHistogramCount);
      DCHECK(kSdchPacketHistogramCount > 4);
      if (packet_times_.size() <= 4)
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_1st_To_2nd_c",
                                  packet_times_[1] - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_2nd_To_3rd_c",
                                  packet_times_[2] - packet_times_[1],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_3rd_To_4th_c",
                                  packet_times_[3] - packet_times_[2],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Decode_4th_To_5th_c",
                                  packet_times_[4] - packet_times_[3],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      return;
    }
    case SDCH_PASSTHROUGH: {
      // Despite advertising a dictionary, we handled non-sdch compressed
      // content.
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_Latency_F_a",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_COUNTS_100("Sdch3.Network_Pass-through_Packets_b",
                               observed_packet_count_);
      if (packet_times_.empty())
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_1st_To_Last_a",
                                  final_packet_time_ - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      DCHECK(max_packets_timed_ >= kSdchPacketHistogramCount);
      DCHECK(kSdchPacketHistogramCount > 4);
      if (packet_times_.size() <= 4)
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_1st_To_2nd_c",
                                  packet_times_[1] - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_2nd_To_3rd_c",
                                  packet_times_[2] - packet_times_[1],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_3rd_To_4th_c",
                                  packet_times_[3] - packet_times_[2],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Network_Pass-through_4th_To_5th_c",
                                  packet_times_[4] - packet_times_[3],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      return;
    }

    case SDCH_EXPERIMENT_DECODE: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Decode",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      // We already provided interpacket histograms above in the SDCH_DECODE
      // case, so we don't need them here.
      return;
    }
    case SDCH_EXPERIMENT_HOLDBACK: {
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_1st_To_Last_a",
                                  final_packet_time_ - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);

      DCHECK(max_packets_timed_ >= kSdchPacketHistogramCount);
      DCHECK(kSdchPacketHistogramCount > 4);
      if (packet_times_.size() <= 4)
        return;
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_1st_To_2nd_c",
                                  packet_times_[1] - packet_times_[0],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_2nd_To_3rd_c",
                                  packet_times_[2] - packet_times_[1],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_3rd_To_4th_c",
                                  packet_times_[3] - packet_times_[2],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      UMA_HISTOGRAM_CLIPPED_TIMES("Sdch3.Experiment_Holdback_4th_To_5th_c",
                                  packet_times_[4] - packet_times_[3],
                                  base::TimeDelta::FromMilliseconds(1),
                                  base::TimeDelta::FromSeconds(10), 100);
      return;
    }
    default:
      NOTREACHED();
      return;
  }
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
  // set by net::URLRequest before it calls our Start method.
  request_->response_info_.response_time = Time::Now();
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
    scoped_refptr<net::AuthChallengeInfo> auth_info;
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
  if (request_->status().is_success()) {
    SetupFilter();

    // Check if this content appears to be compressible.
    std::string mime_type;
    if (GetMimeType(&mime_type) &&
        (net::IsSupportedJavascriptMimeType(mime_type.c_str()) ||
        net::IsSupportedNonImageMimeType(mime_type.c_str()))) {
      is_compressible_content_ = true;
    }
  }

  if (!filter_.get()) {
    std::string content_length;
    request_->GetResponseHeaderByName("content-length", &content_length);
    if (!content_length.empty())
      base::StringToInt64(content_length, &expected_content_size_);
  } else {
    // Chrome today only sends "Accept-Encoding" for compression schemes.
    // So, if there is a filter on the response, we know that the content
    // was compressed.
    is_compressed_ = true;
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

  RecordCompressionHistograms();

  if (is_profiling() && metrics_->total_bytes_read_ > 0) {
    // There are valid IO statistics. Fill in other fields of metrics for
    // profiling consumers to retrieve information.
    metrics_->original_url_.reset(new GURL(request_->original_url()));
    metrics_->end_time_ = TimeTicks::Now();
    metrics_->success_ = status.is_success();

    if (!(request_->original_url() == request_->url())) {
      metrics_->url_.reset(new GURL(request_->url()));
    }
  } else {
    metrics_.reset();
  }


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
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestJob::CompleteNotifyDone));
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
                                net::ERR_ABORTED));
  }
}

void URLRequestJob::NotifyRestartRequired() {
  DCHECK(!has_handled_response_);
  if (GetStatus().status() != URLRequestStatus::CANCELED)
    request_->Restart();
}

bool URLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size,
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
                   net::ERR_CONTENT_DECODING_FAILED));
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

void URLRequestJob::EnablePacketCounting(size_t max_packets_timed) {
  if (max_packets_timed_ < max_packets_timed)
    max_packets_timed_ = max_packets_timed;
  packet_timing_enabled_ = true;
}

const URLRequestStatus URLRequestJob::GetStatus() {
  if (request_)
    return request_->status();
  // If the request is gone, we must be cancelled.
  return URLRequestStatus(URLRequestStatus::CANCELED,
                          net::ERR_ABORTED);
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
    net::IOBuffer* stream_buffer = filter_->stream_buffer();
    int stream_buffer_size = filter_->stream_buffer_size();
    rv = ReadRawDataHelper(stream_buffer, stream_buffer_size, bytes_read);
  }
  return rv;
}

bool URLRequestJob::ReadRawDataHelper(net::IOBuffer* buf, int buf_size,
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
  if (rv != net::OK)
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
  if (is_profiling()) {
    ++(metrics_->number_of_read_IO_);
    metrics_->total_bytes_read_ += bytes_read;
  }
  filter_input_byte_count_ += bytes_read;
  UpdatePacketReadTimes();  // Facilitate stats recording if it is active.
  g_url_request_job_tracker.OnBytesRead(this, raw_read_buffer_->data(),
                                        bytes_read);
}

bool URLRequestJob::FilterHasData() {
    return filter_.get() && filter_->stream_data_len();
}

void URLRequestJob::UpdatePacketReadTimes() {
  if (!packet_timing_enabled_)
    return;

  if (filter_input_byte_count_ <= bytes_observed_in_packets_) {
    DCHECK(filter_input_byte_count_ == bytes_observed_in_packets_);
    return;  // No new bytes have arrived.
  }

  if (!bytes_observed_in_packets_)
    request_time_snapshot_ = GetRequestTime();

  final_packet_time_ = base::Time::Now();
  const size_t kTypicalPacketSize = 1430;
  while (filter_input_byte_count_ > bytes_observed_in_packets_) {
    ++observed_packet_count_;
    if (max_packets_timed_ > packet_times_.size()) {
      packet_times_.push_back(final_packet_time_);
      DCHECK(static_cast<size_t>(observed_packet_count_) ==
             packet_times_.size());
    }
    bytes_observed_in_packets_ += kTypicalPacketSize;
  }
  // Since packets may not be full, we'll remember the number of bytes we've
  // accounted for in packets thus far.
  bytes_observed_in_packets_ = filter_input_byte_count_;
}

// The common type of histogram we use for all compression-tracking histograms.
#define COMPRESSION_HISTOGRAM(name, sample) \
    do { \
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.Compress." name, sample, \
                                  500, 1000000, 100); \
    } while(0)

void URLRequestJob::RecordCompressionHistograms() {
  if (IsCachedContent() ||          // Don't record cached content
      !GetStatus().is_success() ||  // Don't record failed content
      !is_compressible_content_ ||  // Only record compressible content
      !prefilter_bytes_read_)       // Zero-byte responses aren't useful.
    return;

  // Miniature requests aren't really compressible.  Don't count them.
  const int kMinSize = 16;
  if (prefilter_bytes_read_ < kMinSize)
    return;

  // Only record for http or https urls.
  bool is_http = request_->url().SchemeIs("http");
  bool is_https = request_->url().SchemeIs("https");
  if (!is_http && !is_https)
    return;

  const net::HttpResponseInfo& response = request_->response_info_;
  int compressed_B = prefilter_bytes_read_;
  int decompressed_B = postfilter_bytes_read_;

  // We want to record how often downloaded resources are compressed.
  // But, we recognize that different protocols may have different
  // properties.  So, for each request, we'll put it into one of 3
  // groups:
  //      a) SSL resources
  //         Proxies cannot tamper with compression headers with SSL.
  //      b) Non-SSL, loaded-via-proxy resources
  //         In this case, we know a proxy might have interfered.
  //      c) Non-SSL, loaded-without-proxy resources
  //         In this case, we know there was no explicit proxy.  However,
  //         it is possible that a transparent proxy was still interfering.
  //
  // For each group, we record the same 3 histograms.

  if (is_https) {
    if (is_compressed_) {
      COMPRESSION_HISTOGRAM("SSL.BytesBeforeCompression", compressed_B);
      COMPRESSION_HISTOGRAM("SSL.BytesAfterCompression", decompressed_B);
    } else {
      COMPRESSION_HISTOGRAM("SSL.ShouldHaveBeenCompressed", decompressed_B);
    }
    return;
  }

  if (response.was_fetched_via_proxy) {
    if (is_compressed_) {
      COMPRESSION_HISTOGRAM("Proxy.BytesBeforeCompression", compressed_B);
      COMPRESSION_HISTOGRAM("Proxy.BytesAfterCompression", decompressed_B);
    } else {
      COMPRESSION_HISTOGRAM("Proxy.ShouldHaveBeenCompressed", decompressed_B);
    }
    return;
  }

  if (is_compressed_) {
    COMPRESSION_HISTOGRAM("NoProxy.BytesBeforeCompression", compressed_B);
    COMPRESSION_HISTOGRAM("NoProxy.BytesAfterCompression", decompressed_B);
  } else {
    COMPRESSION_HISTOGRAM("NoProxy.ShouldHaveBeenCompressed", decompressed_B);
  }
}

}  // namespace net
