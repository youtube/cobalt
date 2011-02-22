// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/http/http_request_info.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_throttler_entry_interface.h"

namespace net {

class HttpResponseInfo;
class HttpTransaction;
class URLRequestContext;

// A net::URLRequestJob subclass that is built on top of HttpTransaction.  It
// provides an implementation for both HTTP and HTTPS.
class URLRequestHttpJob : public URLRequestJob {
 public:
  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme);

 protected:
  explicit URLRequestHttpJob(URLRequest* request);

  // Shadows URLRequestJob's version of this method so we can grab cookies.
  void NotifyHeadersComplete();

  void DestroyTransaction();
  void StartTransaction();
  void AddExtraHeaders();
  void AddCookieHeaderAndStart();
  void SaveCookiesAndNotifyHeadersComplete();
  void SaveNextCookie();
  void FetchResponseCookies(const HttpResponseInfo* response_info,
                            std::vector<std::string>* cookies);

  // Process the Strict-Transport-Security header, if one exists.
  void ProcessStrictTransportSecurityHeader();

  void OnCanGetCookiesCompleted(int result);
  void OnCanSetCookieCompleted(int result);
  void OnStartCompleted(int result);
  void OnReadCompleted(int result);

  bool ShouldTreatAsCertificateError(int result);

  void RestartTransactionWithAuth(const string16& username,
                                  const string16& password);

  // Overridden from URLRequestJob:
  virtual void SetUpload(UploadData* upload);
  virtual void SetExtraRequestHeaders(const HttpRequestHeaders& headers);
  virtual void Start();
  virtual void Kill();
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(HttpResponseInfo* info);
  virtual bool GetResponseCookies(std::vector<std::string>* cookies);
  virtual int GetResponseCode() const;
  virtual bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_type);
  virtual bool IsCachedContent() const;
  virtual bool IsSdchResponse() const;
  virtual bool IsSafeRedirect(const GURL& location);
  virtual bool NeedsAuth();
  virtual void GetAuthChallengeInfo(scoped_refptr<AuthChallengeInfo>*);
  virtual void SetAuth(const string16& username,
                       const string16& password);
  virtual void CancelAuth();
  virtual void ContinueWithCertificate(X509Certificate* client_cert);
  virtual void ContinueDespiteLastError();
  virtual bool ReadRawData(IOBuffer* buf, int buf_size, int *bytes_read);
  virtual void StopCaching();
  virtual HostPortPair GetSocketAddress() const;

  // Keep a reference to the url request context to be sure it's not deleted
  // before us.
  scoped_refptr<URLRequestContext> context_;

  HttpRequestInfo request_info_;
  const HttpResponseInfo* response_info_;

  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;

  // Auth states for proxy and origin server.
  AuthState proxy_auth_state_;
  AuthState server_auth_state_;

  string16 username_;
  string16 password_;

  CompletionCallbackImpl<URLRequestHttpJob> can_get_cookies_callback_;
  CompletionCallbackImpl<URLRequestHttpJob> can_set_cookie_callback_;
  CompletionCallbackImpl<URLRequestHttpJob> start_callback_;
  CompletionCallbackImpl<URLRequestHttpJob> read_callback_;

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
  virtual ~URLRequestHttpJob();

  ScopedRunnableMethodFactory<URLRequestHttpJob> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
