// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "net/base/host_port_pair.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace cobalt {
namespace speech {

class URLFetcherFake : public net::URLFetcher {
 public:
  URLFetcherFake(const GURL& url, net::URLFetcher::RequestType request_type,
                 net::URLFetcherDelegate* delegate);
  virtual ~URLFetcherFake();

  // net::URLFetcher implementation:
  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_content) override {
    NOTREACHED();
  }
  void SetUploadFilePath(
      const std::string& upload_content_type, const base::FilePath& file_path,
      uint64_t range_offset, uint64_t range_length,
      scoped_refptr<base::TaskRunner> file_task_runner) override{};
  void SetUploadStreamFactory(
      const std::string& upload_content_type,
      const CreateUploadStreamCallback& callback) override{};
  void SetChunkedUpload(const std::string& upload_content_type) override;
  void AppendChunkToUpload(const std::string& data,
                           bool is_last_chunk) override;
  void SetLoadFlags(int load_flags) override { NOTREACHED(); }
  void SetAllowCredentials(bool allow_credentials) override {}
  int GetLoadFlags() const override {
    NOTREACHED();
    return 0;
  }
  void SetReferrer(const std::string& referrer) override { NOTREACHED(); }
  void SetReferrerPolicy(
      net::URLRequest::ReferrerPolicy referrer_policy) override {}
  void SetExtraRequestHeaders(
      const std::string& extra_request_headers) override {
    NOTREACHED();
  }
  void AddExtraRequestHeader(const std::string& header_line) override {
    NOTREACHED();
  }
  void SetRequestContext(
      net::URLRequestContextGetter* request_context_getter) override;
  void SetInitiator(const base::Optional<url::Origin>& initiator) override {}
  void SetURLRequestUserData(
      const void* key,
      const CreateDataCallback& create_data_callback) override {
    NOTREACHED();
  }
  void SetStopOnRedirect(bool stop_on_redirect) override { NOTREACHED(); }
  void SetAutomaticallyRetryOn5xx(bool retry) override { NOTREACHED(); }
  void SaveResponseToFileAtPath(
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) override {}
  void SaveResponseToTemporaryFile(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) override {}
  void SaveResponseWithWriter(
      std::unique_ptr<net::URLFetcherResponseWriter> response_writer) override {
    response_data_writer_ = std::move(response_writer);
  }
  net::URLFetcherResponseWriter* GetResponseWriter() const override {
    return response_data_writer_.get();
  }
  void SetMaxRetriesOn5xx(int max_retries) override { NOTREACHED(); }
  int GetMaxRetriesOn5xx() const override {
    NOTREACHED();
    return 0;
  }
  base::TimeDelta GetBackoffDelay() const override {
    NOTREACHED();
    return base::TimeDelta();
  }
  void SetAutomaticallyRetryOnNetworkChanges(int max_retries) override {
    NOTREACHED();
  }
  net::HttpResponseHeaders* GetResponseHeaders() const override {
    NOTREACHED();
    return NULL;
  }
  net::HostPortPair GetSocketAddress() const override {
    NOTREACHED();
    return net::HostPortPair();
  }
  const net::ProxyServer& ProxyServerUsed() const override {
    NOTREACHED();
    return proxy_server_;
  }
  bool WasFetchedViaProxy() const override {
    NOTREACHED();
    return false;
  }
  bool WasCached() const override {
    NOTREACHED();
    return false;
  }
  int64_t GetReceivedResponseContentLength() const override {
    NOTREACHED();
    return 1;
  }
  int64_t GetTotalReceivedBytes() const override {
    NOTREACHED();
    return 1;
  }
  void Start() override;
  const GURL& GetOriginalURL() const override { return original_url_; }
  const GURL& GetURL() const override { return original_url_; }
  const net::URLRequestStatus& GetStatus() const override;
  int GetResponseCode() const override;
  void ReceivedContentWasMalformed() override { NOTREACHED(); }
  bool GetResponseAsString(std::string* out_response_string) const override {
    NOTREACHED();
    return false;
  }
  bool GetResponseAsFilePath(bool take_ownership,
                             base::FilePath* out_response_path) const override {
    NOTREACHED();
    return false;
  }

 private:
  void OnURLFetchDownloadData();

  GURL original_url_;
  net::URLFetcherDelegate* delegate_;
  bool is_chunked_upload_;
  int download_index_;
  net::URLRequestStatus fake_status_;
  base::Optional<base::RepeatingTimer> download_timer_;
  net::ProxyServer proxy_server_;
  std::unique_ptr<net::URLFetcherResponseWriter> response_data_writer_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace speech
}  // namespace cobalt

#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // COBALT_SPEECH_URL_FETCHER_FAKE_H_
