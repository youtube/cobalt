// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "components/update_client/net/network_impl_cobalt.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace update_client {

namespace {

class NetworkFetcherCobalt : public NetworkFetcher {
 public:
  explicit NetworkFetcherCobalt(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
      : shared_url_network_factory_(shared_url_network_factory) {}

  ~NetworkFetcherCobalt() override = default;

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = "POST";
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    for (const auto& [name, value] : post_additional_headers) {
      resource_request->headers.SetHeader(name, value);
    }
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request),
        net::DefineNetworkTrafficAnnotation("test", "test"));
    simple_url_loader_->AttachStringForUpload(post_data, content_type);
    simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
        [](ResponseStartedCallback response_started_callback,
           const GURL& final_url,
           const network::mojom::URLResponseHead& response_head) {
          response_started_callback.Run(
              response_head.headers ? response_head.headers->response_code()
                                    : -1,
              response_head.content_length);
        },
        response_started_callback));
    simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
        [](ProgressCallback progress_callback, uint64_t current) {
          progress_callback.Run(static_cast<int64_t>(current));
        },
        progress_callback));
    simple_url_loader_->DownloadToString(
        shared_url_network_factory_.get(),
        base::BindOnce(
            [](NetworkFetcherCobalt* fetcher,
               PostRequestCompleteCallback post_request_complete_callback,
               std::unique_ptr<std::string> response_body) {
              std::optional<std::string> optional_response_body;
              int net_error = fetcher->simple_url_loader_->NetError();
              if (net_error == net::OK && response_body) {
                optional_response_body = std::move(*response_body);
              }
              std::string etag, cup_proof, cookie;
              int64_t retry_after = -1;
              if (fetcher->simple_url_loader_->ResponseInfo() &&
                  fetcher->simple_url_loader_->ResponseInfo()->headers) {
                auto* headers = fetcher->simple_url_loader_->ResponseInfo()->headers.get();
                headers->EnumerateHeader(nullptr, kHeaderEtag, &etag);
                headers->EnumerateHeader(nullptr, kHeaderXCupServerProof, &cup_proof);
                headers->EnumerateHeader(nullptr, kHeaderCookie, &cookie);
                retry_after = headers->GetInt64HeaderValue(kHeaderXRetryAfter);
              }
              std::move(post_request_complete_callback)
                  .Run(std::move(optional_response_body),
                       fetcher->simple_url_loader_->NetError(), etag, cup_proof,
                       cookie, retry_after);
            },
            base::Unretained(this),
            std::move(post_request_complete_callback)),
        1024 * 1024);
  }

  void DownloadToString(
      const GURL& url,
      std::string* dst,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToStringCompleteCallback download_to_string_complete_callback) override {
    dst_str_ = dst;
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = "GET";
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request),
        net::DefineNetworkTrafficAnnotation("test", "test"));
    simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
        [](ResponseStartedCallback response_started_callback,
           const GURL& final_url,
           const network::mojom::URLResponseHead& response_head) {
          response_started_callback.Run(
              response_head.headers ? response_head.headers->response_code()
                                    : -1,
              response_head.content_length);
        },
        response_started_callback));
    simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
        [](ProgressCallback progress_callback, uint64_t current) {
          progress_callback.Run(static_cast<int64_t>(current));
        },
        progress_callback));
    simple_url_loader_->DownloadToString(
        shared_url_network_factory_.get(),
        base::BindOnce(
            [](NetworkFetcherCobalt* fetcher,
               DownloadToStringCompleteCallback download_to_string_complete_callback,
               std::unique_ptr<std::string> response_body) {
              if (response_body) {
                *fetcher->dst_str_ = std::move(*response_body);
              } else {
                fetcher->dst_str_->clear();
              }
              std::move(download_to_string_complete_callback)
                  .Run(fetcher->dst_str_, fetcher->simple_url_loader_->NetError(),
                       fetcher->simple_url_loader_->GetContentSize());
            },
            base::Unretained(this),
            std::move(download_to_string_complete_callback)),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  }

  void Cancel() override {
    simple_url_loader_.reset();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  base::raw_ptr<std::string> dst_str_;
};

}  // namespace

NetworkFetcherCobaltFactory::NetworkFetcherCobaltFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
    : shared_url_network_factory_(shared_url_network_factory) {}

NetworkFetcherCobaltFactory::~NetworkFetcherCobaltFactory() = default;

std::unique_ptr<NetworkFetcher> NetworkFetcherCobaltFactory::Create() const {
  return std::make_unique<NetworkFetcherCobalt>(shared_url_network_factory_);
}

}  // namespace update_client
