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
                     const std::string& /*upload_content*/) override {
    NOTREACHED();
  }
  void SetChunkedUpload(const std::string& /*upload_content_type*/) override;
  void AppendChunkToUpload(const std::string& data,
                           bool is_last_chunk) override;
  void SetLoadFlags(int /*load_flags*/) override { NOTREACHED(); }
  int GetLoadFlags() const override {
    NOTREACHED();
    return 0;
  }
  void SetReferrer(const std::string& /*referrer*/) override { NOTREACHED(); }
  void SetExtraRequestHeaders(
      const std::string& /*extra_request_headers*/) override {
    NOTREACHED();
  }
  void AddExtraRequestHeader(const std::string& /*header_line*/) override {
    NOTREACHED();
  }
  void GetExtraRequestHeaders(
      net::HttpRequestHeaders* /*headers*/) const override {
    NOTREACHED();
  }
  void SetRequestContext(
      net::URLRequestContextGetter* request_context_getter) override;
  void SetFirstPartyForCookies(
      const GURL& /*first_party_for_cookies*/) override {
    NOTREACHED();
  }
  void SetURLRequestUserData(
      const void* /*key*/,
      const CreateDataCallback& /*create_data_callback*/) override {
    NOTREACHED();
  }
  void SetStopOnRedirect(bool /*stop_on_redirect*/) override { NOTREACHED(); }
  void SetAutomaticallyRetryOn5xx(bool /*retry*/) override { NOTREACHED(); }
  void SetMaxRetriesOn5xx(int /*max_retries*/) override { NOTREACHED(); }
  int GetMaxRetriesOn5xx() const override {
    NOTREACHED();
    return 0;
  }
  base::TimeDelta GetBackoffDelay() const override {
    NOTREACHED();
    return base::TimeDelta();
  }
  void SetAutomaticallyRetryOnNetworkChanges(int /*max_retries*/) override {
    NOTREACHED();
  }
  void SaveResponseToFileAtPath(
      const FilePath& /*file_path*/,
      scoped_refptr<base::TaskRunner> /*file_task_runner*/) override {
    NOTREACHED();
  }
  void SaveResponseToTemporaryFile(
      scoped_refptr<base::TaskRunner> /*file_task_runner*/) override {
    NOTREACHED();
  }
  void DiscardResponse() override { NOTREACHED(); }
  net::HttpResponseHeaders* GetResponseHeaders() const override {
    NOTREACHED();
    return NULL;
  }
  net::HostPortPair GetSocketAddress() const override {
    NOTREACHED();
    return net::HostPortPair();
  }
  bool WasFetchedViaProxy() const override {
    NOTREACHED();
    return false;
  }
  void Start() override;
  const GURL& GetOriginalURL() const override { return original_url_; }
  const GURL& GetURL() const override { return original_url_; }
  const net::URLRequestStatus& GetStatus() const override;
  int GetResponseCode() const override;
  const net::ResponseCookies& GetCookies() const override {
    NOTREACHED();
    return fake_cookies_;
  }
  bool FileErrorOccurred(
      base::PlatformFileError* /*out_error_code*/) const override {
    NOTREACHED();
    return false;
  }
  void ReceivedContentWasMalformed() override { NOTREACHED(); }
  bool GetResponseAsString(
      std::string* /*out_response_string*/) const override {
    NOTREACHED();
    return false;
  }
  bool GetResponseAsFilePath(bool /*take_ownership*/,
                             FilePath* /*out_response_path*/) const override {
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
