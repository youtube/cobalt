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

#include "cobalt/updater/network_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_throttle.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

// TODO(b/448186580): consider Replace LOG with D(V)LOG throughout this file
// TODO(b/449223391): Revist and document the usage of base::Unretained()

namespace {

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("cobalt_updater_network_fetcher",
                                        "cobalt_updater_network_fetcher");

std::string GetStringHeader(const network::SimpleURLLoader* simple_url_loader,
                            const char* header_name) {
  CHECK(simple_url_loader);

  const auto* response_info = simple_url_loader->ResponseInfo();
  if (!response_info || !response_info->headers) {
    return {};
  }

  std::string header_value;
  return response_info->headers->EnumerateHeader(nullptr, header_name,
                                                 &header_value)
             ? header_value
             : std::string{};
}

int64_t GetInt64Header(const network::SimpleURLLoader* simple_url_loader,
                       const char* header_name) {
  CHECK(simple_url_loader);

  const auto* response_info = simple_url_loader->ResponseInfo();
  if (!response_info || !response_info->headers) {
    return -1;
  }

  return response_info->headers->GetInt64HeaderValue(header_name);
}

}  // namespace

namespace cobalt {
namespace updater {

NetworkFetcher::NetworkFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {
  LOG(INFO) << "cobalt::updater::NetworkFetcher::NetworkFetcher";
}

NetworkFetcher::~NetworkFetcher() {
  LOG(INFO) << "cobalt::updater::NetworkFetcher::~NetworkFetcher";
}

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  CHECK(!simple_url_loader_);

  LOG(INFO) << "NetworkFetcher::PostRequest";
  LOG(INFO) << "PostRequest url = " << url;
  LOG(INFO) << "PostRequest post_data = " << post_data;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  for (const auto& header : post_additional_headers) {
    resource_request->headers.SetHeader(header.first, header.second);
  }
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  if (network::SimpleURLLoaderThrottle::IsBatchingEnabled(traffic_annotation)) {
    simple_url_loader_->SetAllowBatching();
  }
  simple_url_loader_->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // The `Content-Type` header set by |AttachStringForUpload| overwrites any
  // `Content-Type` header present in the |ResourceRequest| above.
  simple_url_loader_->AttachStringForUpload(post_data, content_type);
  simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcher::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
      &NetworkFetcher::OnProgressCallback, base::Unretained(this),
      std::move(progress_callback)));
  constexpr size_t kMaxResponseSize = 1024 * 1024;
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](const network::SimpleURLLoader* simple_url_loader,
             PostRequestCompleteCallback post_request_complete_callback,
             std::unique_ptr<std::string> response_body) {
            LOG(INFO) << "post_request_complete_callback, response_body="
                      << response_body->c_str();
            std::move(post_request_complete_callback)
                .Run(std::move(response_body), simple_url_loader->NetError(),
                     GetStringHeader(simple_url_loader, kHeaderEtag),
                     GetStringHeader(simple_url_loader, kHeaderXCupServerProof),
                     GetInt64Header(simple_url_loader, kHeaderXRetryAfter));
          },
          simple_url_loader_.get(), std::move(post_request_complete_callback)),
      kMaxResponseSize);
}

#if defined(IN_MEMORY_UPDATES)
void NetworkFetcher::DownloadToString(
    const GURL& url,
    std::string* dst,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToStringCompleteCallback download_to_string_complete_callback) {
  LOG(INFO) << "NetworkFetcher::DownloadToString, url = " << url;
  CHECK(dst != nullptr);
  dst_str_ = dst;

  CHECK(!simple_url_loader_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcher::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
      &NetworkFetcher::OnProgressCallback, base::Unretained(this),
      std::move(progress_callback)));
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NetworkFetcher::OnDownloadToStringComplete,
                     base::Unretained(this),
                     std::move(download_to_string_complete_callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void NetworkFetcher::OnDownloadToStringComplete(
    DownloadToStringCompleteCallback download_to_string_complete_callback,
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    LOG(ERROR) << "DownloadToString failed to get response from a string";
    dst_str_->clear();
  } else {
    *dst_str_ = std::move(*response_body);
  }
  std::move(download_to_string_complete_callback)
      .Run(dst_str_, simple_url_loader_->NetError(),
           simple_url_loader_->GetContentSize());
}

#else
void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  LOG(INFO) << "NetworkFetcher::DownloadToFile, file_path = "
            << file_path.value().c_str() << ", url = " << url;
  CHECK(!simple_url_loader_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcher::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
      &NetworkFetcher::OnProgressCallback, base::Unretained(this),
      std::move(progress_callback)));
  simple_url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(
          [](const network::SimpleURLLoader* simple_url_loader,
             DownloadToFileCompleteCallback download_to_file_complete_callback,
             // A base::FilePath parameter is needed to match the callback
             // definition.
             base::FilePath file_path) {
            std::move(download_to_file_complete_callback)
                .Run(simple_url_loader->NetError(),
                     simple_url_loader->GetContentSize());
          },
          simple_url_loader_.get(),
          std::move(download_to_file_complete_callback)),
      file_path);
}
#endif

void NetworkFetcher::Cancel() {
  LOG(INFO) << "cobalt::updater::NetworkFetcher::Cancel";
  simple_url_loader_.reset();
}

void NetworkFetcher::OnResponseStartedCallback(
    ResponseStartedCallback response_started_callback,
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  LOG(INFO) << "NetworkFetcher::OnResponseStartedCallback";
  std::move(response_started_callback)
      .Run(response_head.headers ? response_head.headers->response_code() : -1,
           response_head.content_length);
}

void NetworkFetcher::OnProgressCallback(ProgressCallback progress_callback,
                                        uint64_t current) {
  progress_callback.Run(base::saturated_cast<int64_t>(current));
}

NetworkFetcherFactoryCobalt::NetworkFetcherFactoryCobalt(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

NetworkFetcherFactoryCobalt::~NetworkFetcherFactoryCobalt() = default;

std::unique_ptr<update_client::NetworkFetcher>
NetworkFetcherFactoryCobalt::Create() const {
  return std::make_unique<NetworkFetcher>(url_loader_factory_);
}

}  // namespace updater
}  // namespace cobalt
