// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_request_info.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_throttler_entry_interface.h"

namespace net {

class HttpResponseHeaders;
class HttpResponseInfo;
class HttpTransaction;
class HttpUserAgentSettings;
class UploadDataStream;
class URLRequestContext;

// A URLRequestJob subclass that is built on top of HttpTransaction.  It
// provides an implementation for both HTTP and HTTPS.
class URLRequestHttpJob : public URLRequestJob {
 public:
  static URLRequestJob* Factory(URLRequest* request,
                                NetworkDelegate* network_delegate,
                                const std::string& scheme);

 protected:
  URLRequestHttpJob(URLRequest* request,
                    NetworkDelegate* network_delegate,
                    const HttpUserAgentSettings* http_user_agent_settings);

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

  // Processes the Strict-Transport-Security header, if one exists.
  void ProcessStrictTransportSecurityHeader();

  // Processes the Public-Key-Pins header, if one exists.
  void ProcessPublicKeyPinsHeader();

  // |result| should be net::OK, or the request is canceled.
  void OnHeadersReceivedCallback(int result);
  void OnStartCompleted(int result);
  void OnReadCompleted(int result);
  void NotifyBeforeSendHeadersCallback(int result);

  void RestartTransactionWithAuth(const AuthCredentials& credentials);

  // Overridden from URLRequestJob:
  virtual void SetUpload(UploadDataStream* upload) override;
  virtual void SetExtraRequestHeaders(
      const HttpRequestHeaders& headers) override;
  virtual void Start() override;
  virtual void Kill() override;
  virtual LoadState GetLoadState() const override;
  virtual UploadProgress GetUploadProgress() const override;
  virtual bool GetMimeType(std::string* mime_type) const override;
  virtual bool GetCharset(std::string* charset) override;
  virtual void GetResponseInfo(HttpResponseInfo* info) override;
  virtual bool GetResponseCookies(std::vector<std::string>* cookies) override;
  virtual int GetResponseCode() const override;
  virtual Filter* SetupFilter() const override;
  virtual bool IsSafeRedirect(const GURL& location) override;
  virtual bool NeedsAuth() override;
  virtual void GetAuthChallengeInfo(scoped_refptr<AuthChallengeInfo>*) override;
  virtual void SetAuth(const AuthCredentials& credentials) override;
  virtual void CancelAuth() override;
  virtual void ContinueWithCertificate(X509Certificate* client_cert) override;
  virtual void ContinueDespiteLastError() override;
  virtual bool ReadRawData(IOBuffer* buf, int buf_size,
                           int* bytes_read) override;
  virtual void StopCaching() override;
  virtual void DoneReading() override;
  virtual HostPortPair GetSocketAddress() const override;
  virtual void NotifyURLRequestDestroyed() override;

  HttpRequestInfo request_info_;
  const HttpResponseInfo* response_info_;

  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;
  base::Time response_date_;

  // Auth states for proxy and origin server.
  AuthState proxy_auth_state_;
  AuthState server_auth_state_;
  AuthCredentials auth_credentials_;

  CompletionCallback start_callback_;
  CompletionCallback notify_before_headers_sent_callback_;

  bool read_in_progress_;

  // An URL for an SDCH dictionary as suggested in a Get-Dictionary HTTP header.
  GURL sdch_dictionary_url_;

  scoped_ptr<HttpTransaction> transaction_;

  // This is used to supervise traffic and enforce exponential
  // back-off.  May be NULL.
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

  typedef base::RefCountedData<bool> SharedBoolean;

  class HttpFilterContext;
  class HttpTransactionDelegateImpl;

  virtual ~URLRequestHttpJob();

  void RecordTimer();
  void ResetTimer();

  virtual void UpdatePacketReadTimes() override;
  void RecordPacketStats(FilterContext::StatisticSelector statistic) const;

  void RecordCompressionHistograms();
  bool IsCompressibleContent() const;

  // Starts the transaction if extensions using the webrequest API do not
  // object.
  void StartTransaction();
  // If |result| is net::OK, calls StartTransactionInternal. Otherwise notifies
  // cancellation.
  void MaybeStartTransactionInternal(int result);
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

  // See the implementation for a description of save_next_cookie_running and
  // callback_pending.
  void OnCookieSaved(scoped_refptr<SharedBoolean> save_next_cookie_running,
                     scoped_refptr<SharedBoolean> callback_pending,
                     bool cookie_status);

  // Some servers send the body compressed, but specify the content length as
  // the uncompressed size. If this is the case, we return true in order
  // to request to work around this non-adherence to the HTTP standard.
  // |rv| is the standard return value of a read function indicating the number
  // of bytes read or, if negative, an error code.
  bool ShouldFixMismatchedContentLength(int rv) const;

  // Returns the effective response headers, considering that they may be
  // overridden by |override_response_headers_|.
  HttpResponseHeaders* GetResponseHeaders() const;

  // Override of the private interface of URLRequestJob.
  virtual void OnDetachRequest() override;

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
  base::WeakPtrFactory<URLRequestHttpJob> weak_factory_;

  CompletionCallback on_headers_received_callback_;

  // We allow the network delegate to modify a copy of the response headers.
  // This prevents modifications of headers that are shared with the underlying
  // layers of the network stack.
  scoped_refptr<HttpResponseHeaders> override_response_headers_;

  // Flag used to verify that |this| is not deleted while we are awaiting
  // a callback from the NetworkDelegate. Used as a fail-fast mechanism.
  // True if we are waiting a callback and
  // NetworkDelegate::NotifyURLRequestDestroyed has not been called, yet,
  // to inform the NetworkDelegate that it may not call back.
  bool awaiting_callback_;

  scoped_ptr<HttpTransactionDelegateImpl> http_transaction_delegate_;

  const HttpUserAgentSettings* http_user_agent_settings_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
