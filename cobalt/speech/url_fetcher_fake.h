// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SPEECH_URL_FETCHER_FAKE_H_
#define COBALT_SPEECH_URL_FETCHER_FAKE_H_

#include "cobalt/speech/speech_configuration.h"

#if defined(ENABLE_FAKE_MICROPHONE)

#include <string>

#include "base/optional.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace cobalt {
namespace speech {

class URLFetcherFake : public net::URLFetcher {
 public:
  URLFetcherFake(const GURL& url, net::URLFetcher::RequestType request_type,
                 net::URLFetcherDelegate* delegate);
  virtual ~URLFetcherFake();

  // URLFetcher implementation:
  void SetUploadData(const std::string& /*upload_content_type*/,
                     const std::string& /*upload_content*/) OVERRIDE {
    NOTREACHED();
  }
  void SetChunkedUpload(const std::string& /*upload_content_type*/) OVERRIDE;
  void AppendChunkToUpload(const std::string& data,
                           bool is_last_chunk) OVERRIDE;
  void SetLoadFlags(int /*load_flags*/) OVERRIDE { NOTREACHED(); }
  int GetLoadFlags() const OVERRIDE {
    NOTREACHED();
    return 0;
  }
  void SetReferrer(const std::string& /*referrer*/) OVERRIDE { NOTREACHED(); }
  void SetExtraRequestHeaders(
      const std::string& /*extra_request_headers*/) OVERRIDE {
    NOTREACHED();
  }
  void AddExtraRequestHeader(const std::string& /*header_line*/) OVERRIDE {
    NOTREACHED();
  }
  void GetExtraRequestHeaders(
      net::HttpRequestHeaders* /*headers*/) const OVERRIDE {
    NOTREACHED();
  }
  void SetRequestContext(
      net::URLRequestContextGetter* request_context_getter) OVERRIDE;
  void SetFirstPartyForCookies(
      const GURL& /*first_party_for_cookies*/) OVERRIDE {
    NOTREACHED();
  }
  void SetURLRequestUserData(
      const void* /*key*/,
      const CreateDataCallback& /*create_data_callback*/) OVERRIDE {
    NOTREACHED();
  }
  void SetStopOnRedirect(bool /*stop_on_redirect*/) OVERRIDE { NOTREACHED(); }
  void SetAutomaticallyRetryOn5xx(bool /*retry*/) OVERRIDE { NOTREACHED(); }
  void SetMaxRetriesOn5xx(int /*max_retries*/) OVERRIDE { NOTREACHED(); }
  int GetMaxRetriesOn5xx() const OVERRIDE {
    NOTREACHED();
    return 0;
  }
  base::TimeDelta GetBackoffDelay() const OVERRIDE {
    NOTREACHED();
    return base::TimeDelta();
  }
  void SetAutomaticallyRetryOnNetworkChanges(int /*max_retries*/) OVERRIDE {
    NOTREACHED();
  }
  void SaveResponseToFileAtPath(
      const FilePath& /*file_path*/,
      scoped_refptr<base::TaskRunner> /*file_task_runner*/) OVERRIDE {
    NOTREACHED();
  }
  void SaveResponseToTemporaryFile(
      scoped_refptr<base::TaskRunner> /*file_task_runner*/) OVERRIDE {
    NOTREACHED();
  }
  void DiscardResponse() OVERRIDE { NOTREACHED(); }
  net::HttpResponseHeaders* GetResponseHeaders() const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  net::HostPortPair GetSocketAddress() const OVERRIDE {
    NOTREACHED();
    return net::HostPortPair();
  }
  bool WasFetchedViaProxy() const OVERRIDE {
    NOTREACHED();
    return false;
  }
  void Start() OVERRIDE;
  const GURL& GetOriginalURL() const OVERRIDE { return original_url_; }
  const GURL& GetURL() const OVERRIDE { return original_url_; }
  const net::URLRequestStatus& GetStatus() const OVERRIDE;
  int GetResponseCode() const OVERRIDE;
  const net::ResponseCookies& GetCookies() const OVERRIDE {
    NOTREACHED();
    return fake_cookies_;
  }
  bool FileErrorOccurred(
      base::PlatformFileError* /*out_error_code*/) const OVERRIDE {
    NOTREACHED();
    return false;
  }
  void ReceivedContentWasMalformed() OVERRIDE { NOTREACHED(); }
  bool GetResponseAsString(
      std::string* /*out_response_string*/) const OVERRIDE {
    NOTREACHED();
    return false;
  }
  bool GetResponseAsFilePath(bool /*take_ownership*/,
                             FilePath* /*out_response_path*/) const OVERRIDE {
    NOTREACHED();
    return false;
  }

 private:
  void OnURLFetchDownloadData();

  GURL original_url_;
  net::URLFetcherDelegate* delegate_;
  bool is_chunked_upload_;
  int download_index_;
  net::ResponseCookies fake_cookies_;
  net::URLRequestStatus fake_status_;
  base::optional<base::RepeatingTimer<URLFetcherFake> > download_timer_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // COBALT_SPEECH_URL_FETCHER_FAKE_H_
