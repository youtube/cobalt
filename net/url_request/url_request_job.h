// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/filter.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"


namespace net {

class AuthChallengeInfo;
class HttpRequestHeaders;
class HttpResponseInfo;
class IOBuffer;
class URLRequest;
class URLRequestJobMetrics;
class UploadData;
class URLRequestStatus;
class X509Certificate;

class URLRequestJob : public base::RefCounted<URLRequestJob>,
                      public FilterContext {
 public:
  // When histogramming results related to SDCH and/or an SDCH latency test, the
  // number of packets for which we need to record arrival times so as to
  // calculate interpacket latencies.  We currently are only looking at the
  // first few packets, as we're monitoring the impact of the initial TCP
  // congestion window on stalling of transmissions.
  static const size_t kSdchPacketHistogramCount = 5;

  explicit URLRequestJob(net::URLRequest* request);

  // Returns the request that owns this job. THIS POINTER MAY BE NULL if the
  // request was destroyed.
  net::URLRequest* request() const {
    return request_;
  }

  // Sets the upload data, most requests have no upload data, so this is a NOP.
  // Job types supporting upload data will override this.
  virtual void SetUpload(net::UploadData* upload);

  // Sets extra request headers for Job types that support request headers.
  virtual void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers);

  // If any error occurs while starting the Job, NotifyStartError should be
  // called.
  // This helps ensure that all errors follow more similar notification code
  // paths, which should simplify testing.
  virtual void Start() = 0;

  // This function MUST somehow call NotifyDone/NotifyCanceled or some requests
  // will get leaked. Certain callers use that message to know when they can
  // delete their net::URLRequest object, even when doing a cancel. The default
  // Kill implementation calls NotifyCanceled, so it is recommended that
  // subclasses call URLRequestJob::Kill() after doing any additional work.
  //
  // The job should endeavor to stop working as soon as is convenient, but must
  // not send and complete notifications from inside this function. Instead,
  // complete notifications (including "canceled") should be sent from a
  // callback run from the message loop.
  //
  // The job is not obliged to immediately stop sending data in response to
  // this call, nor is it obliged to fail with "canceled" unless not all data
  // was sent as a result. A typical case would be where the job is almost
  // complete and can succeed before the canceled notification can be
  // dispatched (from the message loop).
  //
  // The job should be prepared to receive multiple calls to kill it, but only
  // one notification must be issued.
  virtual void Kill();

  // Called to detach the request from this Job.  Results in the Job being
  // killed off eventually. The job must not use the request pointer any more.
  void DetachRequest();

  // Called to read post-filtered data from this Job, returning the number of
  // bytes read, 0 when there is no more data, or -1 if there was an error.
  // This is just the backend for net::URLRequest::Read, see that function for
  // more info.
  bool Read(net::IOBuffer* buf, int buf_size, int* bytes_read);

  // Stops further caching of this request, if any. For more info, see
  // net::URLRequest::StopCaching().
  virtual void StopCaching();

  // Called to fetch the current load state for the job.
  virtual net::LoadState GetLoadState() const;

  // Called to get the upload progress in bytes.
  virtual uint64 GetUploadProgress() const;

  // Called to fetch the charset for this request.  Only makes sense for some
  // types of requests. Returns true on success.  Calling this on a type that
  // doesn't have a charset will return false.
  virtual bool GetCharset(std::string* charset);

  // Called to get response info.
  virtual void GetResponseInfo(net::HttpResponseInfo* info);

  // Returns the cookie values included in the response, if applicable.
  // Returns true if applicable.
  // NOTE: This removes the cookies from the job, so it will only return
  //       useful results once per job.
  virtual bool GetResponseCookies(std::vector<std::string>* cookies);

  // Called to fetch the encoding types for this request. Only makes sense for
  // some types of requests. Returns true on success. Calling this on a request
  // that doesn't have or specify an encoding type will return false.
  // Returns a array of strings showing the sequential encodings used on the
  // content.
  // For example, encoding_types[0] = FILTER_TYPE_SDCH and encoding_types[1] =
  // FILTER_TYPE_GZIP, means the content was first encoded by sdch, and then
  // result was encoded by gzip.  To decode, a series of filters must be applied
  // in the reverse order (in the above example, ungzip first, and then sdch
  // expand).
  virtual bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_types);

  // Called to setup stream filter for this request. An example of filter is
  // content encoding/decoding.
  void SetupFilter();

  // Called to determine if this response is a redirect.  Only makes sense
  // for some types of requests.  This method returns true if the response
  // is a redirect, and fills in the location param with the URL of the
  // redirect.  The HTTP status code (e.g., 302) is filled into
  // |*http_status_code| to signify the type of redirect.
  //
  // The caller is responsible for following the redirect by setting up an
  // appropriate replacement Job. Note that the redirected location may be
  // invalid, the caller should be sure it can handle this.
  //
  // The default implementation inspects the response_info_.
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // Called to determine if it is okay to redirect this job to the specified
  // location.  This may be used to implement protocol-specific restrictions.
  // If this function returns false, then the net::URLRequest will fail
  // reporting net::ERR_UNSAFE_REDIRECT.
  virtual bool IsSafeRedirect(const GURL& location);

  // Called to determine if this response is asking for authentication.  Only
  // makes sense for some types of requests.  The caller is responsible for
  // obtaining the credentials passing them to SetAuth.
  virtual bool NeedsAuth();

  // Fills the authentication info with the server's response.
  virtual void GetAuthChallengeInfo(
      scoped_refptr<net::AuthChallengeInfo>* auth_info);

  // Resend the request with authentication credentials.
  virtual void SetAuth(const string16& username,
                       const string16& password);

  // Display the error page without asking for credentials again.
  virtual void CancelAuth();

  virtual void ContinueWithCertificate(net::X509Certificate* client_cert);

  // Continue processing the request ignoring the last error.
  virtual void ContinueDespiteLastError();

  void FollowDeferredRedirect();

  // Returns true if the Job is done producing response data and has called
  // NotifyDone on the request.
  bool is_done() const { return done_; }

  // Returns true if the job is doing performance profiling
  bool is_profiling() const { return is_profiling_; }

  // Retrieve the performance measurement of the job. The data is encapsulated
  // with a URLRequestJobMetrics object. The caller owns this object from now
  // on.
  URLRequestJobMetrics* RetrieveMetrics();

  // Get/Set expected content size
  int64 expected_content_size() const { return expected_content_size_; }
  void set_expected_content_size(const int64& size) {
    expected_content_size_ = size;
  }

  // Whether we have processed the response for that request yet.
  bool has_response_started() const { return has_handled_response_; }

  // FilterContext methods:
  // These methods are not applicable to all connections.
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetURL(GURL* gurl) const;
  virtual base::Time GetRequestTime() const;
  virtual bool IsDownload() const;
  virtual bool IsSdchResponse() const;
  virtual bool IsCachedContent() const;
  virtual int64 GetByteReadCount() const;
  virtual int GetResponseCode() const;
  virtual void RecordPacketStats(StatisticSelector statistic) const;

  // Returns the socket address for the connection.
  // See url_request.h for details.
  virtual HostPortPair GetSocketAddress() const;

 protected:
  friend class base::RefCounted<URLRequestJob>;
  virtual ~URLRequestJob();

  // Notifies the job that headers have been received.
  void NotifyHeadersComplete();

  // Notifies the request that the job has completed a Read operation.
  void NotifyReadComplete(int bytes_read);

  // Notifies the request that a start error has occurred.
  void NotifyStartError(const net::URLRequestStatus& status);

  // NotifyDone marks when we are done with a request.  It is really
  // a glorified set_status, but also does internal state checking and
  // job tracking.  It should be called once per request, when the job is
  // finished doing all IO.
  void NotifyDone(const net::URLRequestStatus& status);

  // Some work performed by NotifyDone must be completed on a separate task
  // so as to avoid re-entering the delegate.  This method exists to perform
  // that work.
  void CompleteNotifyDone();

  // Used as an asynchronous callback for Kill to notify the net::URLRequest
  // that we were canceled.
  void NotifyCanceled();

  // Notifies the job the request should be restarted.
  // Should only be called if the job has not started a resposne.
  void NotifyRestartRequired();

  // Called to read raw (pre-filtered) data from this Job.
  // If returning true, data was read from the job.  buf will contain
  // the data, and bytes_read will receive the number of bytes read.
  // If returning true, and bytes_read is returned as 0, there is no
  // additional data to be read.
  // If returning false, an error occurred or an async IO is now pending.
  // If async IO is pending, the status of the request will be
  // URLRequestStatus::IO_PENDING, and buf must remain available until the
  // operation is completed.  See comments on net::URLRequest::Read for more
  // info.
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

  // Informs the filter that data has been read into its buffer
  void FilteredDataRead(int bytes_read);

  // Reads filtered data from the request.  Returns true if successful,
  // false otherwise.  Note, if there is not enough data received to
  // return data, this call can issue a new async IO request under
  // the hood.
  bool ReadFilteredData(int *bytes_read);

  // Facilitate histogramming by turning on packet counting.
  // If called more than once, the largest value will be used.
  void EnablePacketCounting(size_t max_packets_timed);

  // At or near destruction time, a derived class may request that the filters
  // be destroyed so that statistics can be gathered while the derived class is
  // still present to assist in calculations.  This is used by URLRequestHttpJob
  // to get SDCH to emit stats.
  void DestroyFilters() { filter_.reset(); }

  // The status of the job.
  const net::URLRequestStatus GetStatus();

  // Set the status of the job.
  void SetStatus(const net::URLRequestStatus& status);

  // The request that initiated this job. This value MAY BE NULL if the
  // request was released by DetachRequest().
  net::URLRequest* request_;

  // Whether the job is doing performance profiling
  bool is_profiling_;

  // Contains IO performance measurement when profiling is enabled.
  scoped_ptr<URLRequestJobMetrics> metrics_;

  // The number of bytes read before passing to the filter.
  int prefilter_bytes_read_;
  // The number of bytes read after passing through the filter.
  int postfilter_bytes_read_;
  // True when (we believe) the content in this net::URLRequest was
  // compressible.
  bool is_compressible_content_;
  // True when the content in this net::URLRequest was compressed.
  bool is_compressed_;

 private:
  // When data filtering is enabled, this function is used to read data
  // for the filter.  Returns true if raw data was read.  Returns false if
  // an error occurred (or we are waiting for IO to complete).
  bool ReadRawDataForFilter(int *bytes_read);

  // Invokes ReadRawData and records bytes read if the read completes
  // synchronously.
  bool ReadRawDataHelper(net::IOBuffer* buf, int buf_size, int* bytes_read);

  // Called in response to a redirect that was not canceled to follow the
  // redirect. The current job will be replaced with a new job loading the
  // given redirect destination.
  void FollowRedirect(const GURL& location, int http_status_code);

  // Called after every raw read. If |bytes_read| is > 0, this indicates
  // a successful read of |bytes_read| unfiltered bytes. If |bytes_read|
  // is 0, this indicates that there is no additional data to read. If
  // |bytes_read| is < 0, an error occurred and no bytes were read.
  void OnRawReadComplete(int bytes_read);

  // Updates the profiling info and notifies observers that an additional
  // |bytes_read| unfiltered bytes have been read for this job.
  void RecordBytesRead(int bytes_read);

  // Called to query whether there is data available in the filter to be read
  // out.
  bool FilterHasData();

  // Record packet arrival times for possible use in histograms.
  void UpdatePacketReadTimes();

  void RecordCompressionHistograms();

  // Indicates that the job is done producing data, either it has completed
  // all the data or an error has been encountered. Set exclusively by
  // NotifyDone so that it is kept in sync with the request.
  bool done_;

  // Cache the load flags from request_ because it might go away.
  int load_flags_;

  // The data stream filter which is enabled on demand.
  scoped_ptr<Filter> filter_;

  // If the filter filled its output buffer, then there is a change that it
  // still has internal data to emit, and this flag is set.
  bool filter_needs_more_output_space_;

  // When we filter data, we receive data into the filter buffers.  After
  // processing the filtered data, we return the data in the caller's buffer.
  // While the async IO is in progress, we save the user buffer here, and
  // when the IO completes, we fill this in.
  scoped_refptr<net::IOBuffer> filtered_read_buffer_;
  int filtered_read_buffer_len_;

  // We keep a pointer to the read buffer while asynchronous reads are
  // in progress, so we are able to pass those bytes to job observers.
  scoped_refptr<net::IOBuffer> raw_read_buffer_;

  // Used by HandleResponseIfNecessary to track whether we've sent the
  // OnResponseStarted callback and potentially redirect callbacks as well.
  bool has_handled_response_;

  // Expected content size
  int64 expected_content_size_;

  // Set when a redirect is deferred.
  GURL deferred_redirect_url_;
  int deferred_redirect_status_code_;

  //----------------------------------------------------------------------------
  // Data used for statistics gathering in some instances.  This data is only
  // used for histograms etc., and is not required.  It is optionally gathered
  // based on the settings of several control variables.

  // Enable recording of packet arrival times for histogramming.
  bool packet_timing_enabled_;

  // TODO(jar): improve the quality of the gathered info by gathering most times
  // at a lower point in the network stack, assuring we have actual packet
  // boundaries, rather than approximations.  Also note that input byte count
  // as gathered here is post-SSL, and post-cache-fetch, and does not reflect
  // true packet arrival times in such cases.

  // Total number of bytes read from network (or cache) and typically handed
  // to filter to process.  Used to histogram compression ratios, and error
  // recovery scenarios in filters.
  int64 filter_input_byte_count_;

  // The number of bytes that have been accounted for in packets (where some of
  // those packets may possibly have had their time of arrival recorded).
  int64 bytes_observed_in_packets_;

  // Limit on the size of the array packet_times_.  This can be set to
  // zero, and then no packet times will be gathered.
  size_t max_packets_timed_;

  // Arrival times for some of the first few packets.
  std::vector<base::Time> packet_times_;

  // The request time may not be available when we are being destroyed, so we
  // snapshot it early on.
  base::Time request_time_snapshot_;

  // Since we don't save all packet times in packet_times_, we save the
  // last time for use in histograms.
  base::Time final_packet_time_;

  // The count of the number of packets, some of which may not have been timed.
  // We're ignoring overflow, as 1430 x 2^31 is a LOT of bytes.
  int observed_packet_count_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_H_
