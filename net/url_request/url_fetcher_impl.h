// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains URLFetcher, a wrapper around URLRequest that handles
// low-level details like thread safety, ref counting, and incremental buffer
// reading.  This is useful for callers who simply want to get the data from a
// URL and don't care about all the nitty-gritty details.
//
// NOTE(willchan): Only one "IO" thread is supported for URLFetcher.  This is a
// temporary situation.  We will work on allowing support for multiple "io"
// threads per process.

#ifndef NET_URL_REQUEST_URL_FETCHER_IMPL_H_
#define NET_URL_REQUEST_URL_FETCHER_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/url_request/url_fetcher.h"

namespace net {
class URLFetcherCore;
class URLFetcherDelegate;
class URLFetcherFactory;

class NET_EXPORT_PRIVATE URLFetcherImpl : public URLFetcher {
 public:
  // |url| is the URL to send the request to.
  // |request_type| is the type of request to make.
  // |d| the object that will receive the callback on fetch completion.
  URLFetcherImpl(const GURL& url,
                 RequestType request_type,
                 URLFetcherDelegate* d);
  virtual ~URLFetcherImpl();

  // URLFetcher implementation:
  virtual void SetUploadData(const std::string& upload_content_type,
                             const std::string& upload_content) override;
  virtual void SetChunkedUpload(
      const std::string& upload_content_type) override;
  virtual void AppendChunkToUpload(const std::string& data,
                                   bool is_last_chunk) override;
  virtual void SetLoadFlags(int load_flags) override;
  virtual int GetLoadFlags() const override;
  virtual void SetReferrer(const std::string& referrer) override;
  virtual void SetExtraRequestHeaders(
      const std::string& extra_request_headers) override;
  virtual void AddExtraRequestHeader(const std::string& header_line) override;
  virtual void GetExtraRequestHeaders(
      HttpRequestHeaders* headers) const override;
  virtual void SetRequestContext(
      URLRequestContextGetter* request_context_getter) override;
  virtual void SetFirstPartyForCookies(
      const GURL& first_party_for_cookies) override;
  virtual void SetURLRequestUserData(
      const void* key,
      const CreateDataCallback& create_data_callback) override;
  virtual void SetStopOnRedirect(bool stop_on_redirect) override;
  virtual void SetAutomaticallyRetryOn5xx(bool retry) override;
  virtual void SetMaxRetriesOn5xx(int max_retries) override;
  virtual int GetMaxRetriesOn5xx() const override;
  virtual base::TimeDelta GetBackoffDelay() const override;
  virtual void SetAutomaticallyRetryOnNetworkChanges(int max_retries) override;
  virtual void SaveResponseToFileAtPath(
      const FilePath& file_path,
      scoped_refptr<base::TaskRunner> file_task_runner) override;
  virtual void SaveResponseToTemporaryFile(
      scoped_refptr<base::TaskRunner> file_task_runner) override;
#if defined(COBALT)
  virtual void DiscardResponse() override;
#endif
  virtual HttpResponseHeaders* GetResponseHeaders() const override;
  virtual HostPortPair GetSocketAddress() const override;
  virtual bool WasFetchedViaProxy() const override;
  virtual void Start() override;
  virtual const GURL& GetOriginalURL() const override;
  virtual const GURL& GetURL() const override;
  virtual const URLRequestStatus& GetStatus() const override;
  virtual int GetResponseCode() const override;
  virtual const ResponseCookies& GetCookies() const override;
  virtual bool FileErrorOccurred(
      base::PlatformFileError* out_error_code) const override;
  virtual void ReceivedContentWasMalformed() override;
  virtual bool GetResponseAsString(
      std::string* out_response_string) const override;
  virtual bool GetResponseAsFilePath(
      bool take_ownership,
      FilePath* out_response_path) const override;

  static void CancelAll();

  static void SetEnableInterceptionForTests(bool enabled);
  static void SetIgnoreCertificateRequests(bool ignored);

  // TODO(akalin): Make these private again once URLFetcher::Create()
  // is in net/.

  static URLFetcherFactory* factory();

  // Sets the factory used by the static method Create to create a URLFetcher.
  // URLFetcher does not take ownership of |factory|. A value of NULL results
  // in a URLFetcher being created directly.
  //
  // NOTE: for safety, this should only be used through ScopedURLFetcherFactory!
  static void set_factory(URLFetcherFactory* factory);

 protected:
  // Returns the delegate.
  URLFetcherDelegate* delegate() const;

 private:
  friend class URLFetcherTest;

  // Only used by URLFetcherTest, returns the number of URLFetcher::Core objects
  // actively running.
  static int GetNumFetcherCores();

  const scoped_refptr<URLFetcherCore> core_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherImpl);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_IMPL_H_
