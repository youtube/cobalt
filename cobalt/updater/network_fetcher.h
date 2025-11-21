// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_UPDATER_NETWORK_FETCHER_H_
#define COBALT_UPDATER_NETWORK_FETCHER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/network.h"

#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace base {

class FilePath;
class SequencedTaskRunner;

}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace cobalt {
namespace updater {
// The NetworkFetcher class provides an implementation of the
// update_client::NetworkFetcher interface, tailored for Cobalt's updater.
//
// This class is responsible for performing network operations required by
// the updater, such as downloading update payloads or posting update
// requests. It acts as a bridge between the updater logic and the underlying
// network stack, encapsulating the details of network requests and
// responses, and providing a consistent interface for the updater.
//
// A NetworkFetcher instance is created when the updater configurator is
// created, and its lifetime is managed by the updater configurator.
//
// NetworkFetcher is used on a single thread - the updater thread when a
// request (e.g., a file download or a POST request) is made
class NetworkFetcher : public update_client::NetworkFetcher {
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

  explicit NetworkFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~NetworkFetcher() override;

  // update_client::NetworkFetcher interface.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
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

 private:
  // TODO(b/449228491): add thread checker
  // Empty struct to ensure the caller of |HandleError()| knows that |this|
  // may have been destroyed and handles it appropriately.
  struct ReturnWrapper {
    void InvalidateThis() {}
  };

  void OnResponseStartedCallback(
      ResponseStartedCallback response_started_callback,
      const GURL& final_url,
      const network::mojom::URLResponseHead& response_head);

  void OnProgressCallback(ProgressCallback response_started_callback,
                          uint64_t current);

#if defined(IN_MEMORY_UPDATES)
  void OnDownloadToStringComplete(
      DownloadToStringCompleteCallback download_to_string_complete_callback,
      std::unique_ptr<std::string> response_body);
#endif

  static constexpr int kMaxRetriesOnNetworkChange = 3;

#if defined(IN_MEMORY_UPDATES)
  base::raw_ptr<std::string> dst_str_;  // not owned, can't be null
#endif

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

// This class is needed because NetworkFetcherFactory class needs
// an implementation.
class NetworkFetcherFactoryCobalt
    : public update_client::NetworkFetcherFactory {
 public:
  explicit NetworkFetcherFactoryCobalt(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_);

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactoryCobalt() override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_NETWORK_FETCHER_H_
