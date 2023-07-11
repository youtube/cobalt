// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_COBALT_H_
#define COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_COBALT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cobalt/network/network_module.h"
#include "components/update_client/network.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {

class FilePath;
class SingleThreadTaskRunner;

}  // namespace base

namespace update_client {

typedef enum UrlFetcherType {
  kUrlFetcherTypePostRequest,
#if defined(IN_MEMORY_UPDATES)
  kUrlFetcherTypeDownloadToString,
#else
  kUrlFetcherTypeDownloadToFile,
#endif
} UrlFetcherType;

class NetworkFetcherCobaltImpl : public NetworkFetcher,
                                 public net::URLFetcherDelegate {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
#if defined(IN_MEMORY_UPDATES)
  using DownloadToStringCompleteCallback =
      update_client::NetworkFetcher::DownloadToStringCompleteCallback;
#else
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;
#endif

  explicit NetworkFetcherCobaltImpl(
      const cobalt::network::NetworkModule* network_module);
  ~NetworkFetcherCobaltImpl() override;

  // update_client::NetworkFetcher interface.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
#if defined(IN_MEMORY_UPDATES)
  // Does not take ownership of |dst|, which must refer to a valid string that
  // outlives this object.
  void DownloadToString(const GURL& url,
                        std::string* dst,
                        ResponseStartedCallback response_started_callback,
                        ProgressCallback progress_callback,
                        DownloadToStringCompleteCallback
                            download_to_string_complete_callback) override;
#else
  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      ResponseStartedCallback response_started_callback,
                      ProgressCallback progress_callback,
                      DownloadToFileCompleteCallback
                          download_to_file_complete_callback) override;
#endif
  void Cancel() override;

  // net::URLFetcherDelegate interface.
  void OnURLFetchResponseStarted(const net::URLFetcher* source) override;
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override;

 private:
  // Thread checker ensures all calls to the NetworkFetcher are made from the
  // same thread that it is created in.
  THREAD_CHECKER(thread_checker_);

  // Empty struct to ensure the caller of |HandleError()| knows that |this|
  // may have been destroyed and handles it appropriately.
  struct ReturnWrapper {
    void InvalidateThis() {}
  };

  ReturnWrapper HandleError(const std::string& error_message)
      WARN_UNUSED_RESULT;

  void CreateUrlFetcher(const GURL& url,
                        const net::URLFetcher::RequestType request_type);

  void OnPostRequestComplete(const net::URLFetcher* source,
                             const int status_error);

#if defined(IN_MEMORY_UPDATES)
  void OnDownloadToStringComplete(const net::URLFetcher* source,
                                  const int status_error);
#else
  void OnDownloadToFileComplete(const net::URLFetcher* source,
                                const int status_error);
#endif

  static constexpr int kMaxRetriesOnNetworkChange = 3;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  std::unique_ptr<net::URLFetcher> url_fetcher_;

  UrlFetcherType url_fetcher_type_;

  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;
#if defined(IN_MEMORY_UPDATES)
  DownloadToStringCompleteCallback download_to_string_complete_callback_;
  std::string* dst_str_;  // not owned, can't be null
#else
  DownloadToFileCompleteCallback download_to_file_complete_callback_;
#endif

  const cobalt::network::NetworkModule* network_module_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherCobaltImpl);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_COBALT_H_
