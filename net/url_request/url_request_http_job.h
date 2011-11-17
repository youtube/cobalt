// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/base/cookie_store.h"
#include "net/http/http_request_info.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_throttler_entry_interface.h"

namespace net {

class HttpResponseHeaders;
class HttpResponseInfo;
class HttpTransaction;
class URLRequestContext;

// A URLRequestJob subclass that is built on top of HttpTransaction.  It
// provides an implementation for both HTTP and HTTPS.
class URLRequestHttpJob : public URLRequestJob {
 public:
  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme);

 protected:
  explicit URLRequestHttpJob(URLRequest* request);

  // Shadows URLRequestJob's version of this method so we can grab cookies.
  void NotifyHeadersComplete();

  // Shadows URLRequestJob's method so we can record histograms.
  void NotifyDone(const URLRequestStatus& status);

  void DestroyTransaction();

  void AddExtraHeaders();
  void AddCookieHeaderAndStart();
  void SaveCookiesAndNotifyHeadersComplete(int result);
  void SaveNextCookie();
  void FetchResponseCookies(std::vector<std::string>* cookies);

  // Process the Strict-Transport-Security header, if one exists.
  void ProcessStrictTransportSecurityHeader();

  // |result| should be net::OK, or the request is canceled.
  void OnHeadersReceivedCallback(int result);
  void OnStartCompleted(int result);
  void OnReadCompleted(int result);
  void NotifyBeforeSendHeadersCallback(int result);

  void RestartTransactionWithAuth(const AuthCredentials& credentials);

  // Overridden from URLRequestJob:
  virtual void SetUpload(UploadData* upload) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const HttpRequestHeaders& headers) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual LoadState GetLoadState() const OVERRIDE;
  virtual uint64 GetUploadProgress() const OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual void GetResponseInfo(HttpResponseInfo* info) OVERRIDE;
  virtual bool GetResponseCookies(std::vector<std::string>* cookies) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual Filter* SetupFilter() const OVERRIDE;
  virtual bool IsSafeRedirect(const GURL& location) OVERRIDE;
  virtual bool NeedsAuth() OVERRIDE;
  virtual void GetAuthChallengeInfo(scoped_refptr<AuthChallengeInfo>*) OVERRIDE;
  virtual void SetAuth(const AuthCredentials& credentials) OVERRIDE;
  virtual void CancelAuth() OVERRIDE;
  virtual void ContinueWithCertificate(X509Certificate* client_cert) OVERRIDE;
  virtual void ContinueDespiteLastError() OVERRIDE;
  virtual bool ReadRawData(IOBuffer* buf, int buf_size,
                           int *bytes_read) OVERRIDE;
  virtual void StopCaching() OVERRIDE;
  virtual void DoneReading() OVERRIDE;
  virtual HostPortPair GetSocketAddress() const OVERRIDE;
  virtual void NotifyURLRequestDestroyed() OVERRIDE;

  // Keep a reference to the url request context to be sure it's not deleted
  // before us.
  scoped_refptr<const URLRequestContext> context_;

  HttpRequestInfo request_info_;
  const HttpResponseInfo* response_info_;

  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;

  // Auth states for proxy and origin server.
  AuthState proxy_auth_state_;
  AuthState server_auth_state_;
  AuthCredentials auth_credentials_;

  OldCompletionCallbackImpl<URLRequestHttpJob> start_callback_;
  OldCompletionCallbackImpl<URLRequestHttpJob> read_callback_;
  OldCompletionCallbackImpl<URLRequestHttpJob>
      notify_before_headers_sent_callback_;

  bool read_in_progress_;

  // An URL for an SDCH dictionary as suggested in a Get-Dictionary HTTP header.
  GURL sdch_dictionary_url_;

  scoped_ptr<HttpTransaction> transaction_;

  // This is used to supervise traffic and enforce exponential back-off.
  scoped_refptr<URLRequestThrottlerEntryInterface> throttling_entry_;

  // Indicated if an SDCH dictionary was advertised, and hence an SDCH
  // compressed response is expected.  We use this to help detect (accidental?)
  // proxy corruption of a response, which sometimes marks SDCH content as
  // having no content encoding <oops>.
  bool sdch_dictionary_advertised_;

  // For SDCH latency experiments, when we are able to do SDCH, we may enable
  // either an SDCH latency test xor a pass through test.  The following bools
  // indicate what we decided on for this instance.
  bool sdch_test_activated_;  // Advertising a dictionary for sdch.
  bool sdch_test_control_;    // Not even accepting-content sdch.

  // For recording of stats, we need to remember if this is cached content.
  bool is_cached_content_;

 private:
  enum CompletionCause {
    ABORTED,
    FINISHED
  };

  class HttpFilterContext;

  virtual ~URLRequestHttpJob();

  void RecordTimer();
  void ResetTimer();

  virtual void UpdatePacketReadTimes() OVERRIDE;
  void RecordPacketStats(FilterContext::StatisticSelector statistic) const;

  void RecordCompressionHistograms();
  bool IsCompressibleContent() const;

  // Starts the transaction if extensions using the webrequest API do not
  // object.
  void StartTransaction();
  void StartTransactionInternal();

  void RecordPerfHistograms(CompletionCause reason);
  void DoneWithRequest(CompletionCause reason);

  // Callback functions for Cookie Monster
  void DoLoadCookies();
  void CheckCookiePolicyAndLoad(const CookieList& cookie_list);
  void OnCookiesLoaded(
      const std::string& cookie_line,
      const std::vector<CookieStore::CookieInfo>& cookie_infos);
  void DoStartTransaction();
  void OnCookieSaved(bool cookie_status);
  void CookieHandled();

  // Returns the effective response headers, considering that they may be
  // overridden by |override_response_headers_|.
  HttpResponseHeaders* GetResponseHeaders() const;

  base::Time request_creation_time_;

  // Data used for statistics gathering. This data is only used for histograms
  // and is not required. It is only gathered if packet_timing_enabled_ == true.
  //
  // TODO(jar): improve the quality of the gathered info by gathering most times
  // at a lower point in the network stack, assuring we have actual packet
  // boundaries, rather than approximations.  Also note that input byte count
  // as gathered here is post-SSL, and post-cache-fetch, and does not reflect
  // true packet arrival times in such cases.

  // Enable recording of packet arrival times for histogramming.
  bool packet_timing_enabled_;
  bool done_;  // True when we are done doing work.

  // The number of bytes that have been accounted for in packets (where some of
  // those packets may possibly have had their time of arrival recorded).
  int64 bytes_observed_in_packets_;

  // The request time may not be available when we are being destroyed, so we
  // snapshot it early on.
  base::Time request_time_snapshot_;

  // Since we don't save all packet times in packet_times_, we save the
  // last time for use in histograms.
  base::Time final_packet_time_;

  // The start time for the job, ignoring re-starts.
  base::TimeTicks start_time_;

  scoped_ptr<HttpFilterContext> filter_context_;
  ScopedRunnableMethodFactory<URLRequestHttpJob> method_factory_;
  base::WeakPtrFactory<URLRequestHttpJob> weak_ptr_factory_;

  OldCompletionCallbackImpl<URLRequestHttpJob> on_headers_received_callback_;

  scoped_refptr<HttpResponseHeaders> override_response_headers_;

  // Flag used to verify that |this| is not deleted while we are awaiting
  // a callback from the NetworkDelegate. Used as a fail-fast mechanism.
  // True if we are waiting a callback and
  // NetworkDelegate::NotifyURLRequestDestroyed has not been called, yet,
  // to inform the NetworkDelegate that it may not call back.
  bool awaiting_callback_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
