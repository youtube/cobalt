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

#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/browser/splash_cache_reader.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/common/url_constants.h"
#include "cobalt/shell/embedded_resources/embedded_resources.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using switches::kDefaultSplashCacheName;
using switches::kDefaultURL;
using switches::kMaxSplashContentSize;

namespace {
const char kMimeTypeApplicationOctetStream[] = "application/octet-stream";
const char kMimeTypeImagePng[] = "image/png";
const char kMimeTypeTextHtml[] = "text/html";
const char kMimeTypeTextPlain[] = "text/plain";
const char kMimeTypeVideoWebM[] = "video/webm";

// TODO - b/456482732: remove unsafe-inline.
const char kH5vccContentSecurityPolicy[] =
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline'; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "media-src 'self' data: blob: %s:; "
    "connect-src 'self' blob: data: %s:;";
}  // namespace

class H5vccSchemeURLLoader : public network::mojom::URLLoader {
 public:
  H5vccSchemeURLLoader(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      BrowserContext* browser_context,
      const GeneratedResourceMap& resource_map,
      const std::string& splash_domain,
      uint64_t splash_content_size_limit)
      : client_(std::move(client)),
        url_(request.url),
        browser_context_(browser_context),
        splash_domain_(splash_domain),
        splash_content_size_limit_(splash_content_size_limit),
        mime_type_(kMimeTypeApplicationOctetStream) {
    client_.set_disconnect_handler(
        base::BindOnce(&H5vccSchemeURLLoader::OnClientDisconnected,
                       weak_factory_.GetWeakPtr()));
    std::string key = url_.host();
    // The key to query the resource map. It is usually the same as the host,
    // but can be overridden by the "fallback" query param for png/webm
    // resources, so that callers can specify a different file for fallback.
    std::string resource_key = key;

    // For html file, return from embedded resources.
    if (base::EndsWith(key, ".html", base::CompareCase::SENSITIVE)) {
      mime_type_ = kMimeTypeTextHtml;
    } else if (base::EndsWith(key, ".png", base::CompareCase::SENSITIVE)) {
      mime_type_ = kMimeTypeImagePng;
    } else if (base::EndsWith(key, ".webm", base::CompareCase::SENSITIVE)) {
      mime_type_ = kMimeTypeVideoWebM;
    }

    // Splash assets are the png and webm resources defined in the resource
    // map.
    const bool is_splash_asset =
        (mime_type_ == kMimeTypeImagePng || mime_type_ == kMimeTypeVideoWebM) &&
        base::Contains(resource_map, key);
    // Whether to read the splash asset that the WebApp cached.
    const bool supports_splash_caching =
        is_splash_asset &&
        H5vccSchemeURLLoaderFactory::IsCachedSplashFileEnabled();

    // For splash assets, override resource_key with "fallback" query param.
    // If requested key is not found in cache, or the cache isn't read, loader
    // returns the fallback resource.
    std::string fallback;
    if (is_splash_asset &&
        net::GetValueForKeyInQuery(url_, "fallback", &fallback)) {
      LOG(INFO) << "Fallback splash: " << fallback;
      resource_key = std::move(fallback);
    }

    if (const FileContents* file_contents_ptr =
            base::FindOrNull(resource_map, resource_key)) {
      content_ =
          std::string(reinterpret_cast<const char*>(file_contents_ptr->data),
                      file_contents_ptr->size);
    } else {
      LOG(WARNING) << "Resource not found: " << resource_key;
    }

    if (supports_splash_caching && browser_context_) {
      std::string cache_name;
      cache_reader_ = std::make_unique<SplashCacheReader>(
          browser_context_, splash_domain_, splash_content_size_limit_);
      cache_reader_->Read(
          key,
          net::GetValueForKeyInQuery(url_, "cache", &cache_name)
              ? base::UTF8ToUTF16(cache_name)
              : std::u16string(kDefaultSplashCacheName),
          base::BindOnce(&H5vccSchemeURLLoader::OnSplashCacheRead,
                         weak_factory_.GetWeakPtr()));
      return;
    }

    if (content_.empty()) {
      SendNotFoundResponse(key);
      return;
    }

    if (is_splash_asset && !supports_splash_caching) {
      // As on a cache miss: the built-in resource, which the "fallback"
      // query param may have chosen, e.g., the 480p video.
      LOG(INFO) << "Cached splash assets are disabled. Using built-in "
                << resource_key << ".";
      SendResponse(SplashScreenFetchedState::kSkippedCachedAssets);
      return;
    }

    SendResponse();
  }
  ~H5vccSchemeURLLoader() override = default;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

  void OnSplashCacheRead(SplashScreenFetchedState state, std::string content) {
    if (state == SplashScreenFetchedState::kOkCache) {
      content_ = std::move(content);
    }
    // Otherwise, `content_` is the built-in resource, which the "fallback"
    // query param may have chosen, e.g., the 480p video.
    SendResponse(state);
  }

 private:
  void SendNotFoundResponse(const std::string& key) {
    LOG(WARNING) << "URL: " << url_.spec() << ", host: " << key
                 << " not found.";
    content_ = "Resource not found";
    mime_type_ = kMimeTypeTextPlain;
    SendResponse(SplashScreenFetchedState::kErrorOnResourceNotFound,
                 net::HTTP_NOT_FOUND);
  }

  void SendResponse(SplashScreenFetchedState state,
                    int http_status = net::HTTP_OK) {
    RecordSplashScreenFetchedState(state);
    SendResponse(http_status);
  }

  void SendResponse(int http_status = net::HTTP_OK) {
    if (!client_) {
      LOG(ERROR) << "URLLoaderClient is not connected.";
      return;
    }

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->mime_type = mime_type_;
    response_head->content_length = content_.size();

    std::string status_line =
        "HTTP/1.1 " + std::to_string(http_status) + " " +
        net::GetHttpReasonPhrase(static_cast<net::HttpStatusCode>(http_status));
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(status_line);
    response_head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                                      mime_type_);
    response_head->headers->AddHeader(net::HttpRequestHeaders::kContentLength,
                                      std::to_string(content_.size()));
    response_head->headers->AddHeader(
        "Content-Security-Policy",
        base::StringPrintf(kH5vccContentSecurityPolicy, kH5vccEmbeddedScheme,
                           kH5vccEmbeddedScheme));
    // Range requests are not supported.
    response_head->headers->AddHeader("Accept-Ranges", "none");

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = content_.size();
    if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    client_->OnReceiveResponse(std::move(response_head),
                               std::move(consumer_handle), std::nullopt);

    size_t bytes_written = 0;
    MojoResult result = producer_handle->WriteData(
        base::as_byte_span(content_), MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
        bytes_written);

    if (result != MOJO_RESULT_OK || bytes_written != content_.size()) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = content_.size();
    status.encoded_body_length = content_.size();
    client_->OnComplete(status);
  }

  void OnClientDisconnected() {
    // The client is gone, so the loader is about to be destroyed. Cancel any
    // pending cache operations.
    cache_reader_.reset();
  }

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  GURL url_;
  BrowserContext* browser_context_;
  std::string splash_domain_;
  uint64_t splash_content_size_limit_;
  std::string mime_type_;
  std::unique_ptr<SplashCacheReader> cache_reader_;
  std::string content_;
  base::WeakPtrFactory<H5vccSchemeURLLoader> weak_factory_{this};
};

std::optional<std::string>
    H5vccSchemeURLLoaderFactory::global_splash_domain_test_;
std::optional<int>
    H5vccSchemeURLLoaderFactory::global_splash_content_size_test_;
std::optional<bool>
    H5vccSchemeURLLoaderFactory::global_cached_splash_file_enabled_test_;
const GeneratedResourceMap* H5vccSchemeURLLoaderFactory::resource_map_test_ =
    nullptr;

H5vccSchemeURLLoaderFactory::H5vccSchemeURLLoaderFactory(
    BrowserContext* browser_context)
    : splash_domain_(GetSplashDomain()),
      splash_content_size_limit_(GetSplashContentSizeLimit()),
      browser_context_(browser_context) {
  if (resource_map_test_) {
    resource_map_ = *resource_map_test_;
  } else {
    LoaderEmbeddedResources::GenerateMap(resource_map_);
  }
}

H5vccSchemeURLLoaderFactory::~H5vccSchemeURLLoaderFactory() = default;

// static
std::string H5vccSchemeURLLoaderFactory::GetSplashDomain() {
  return url::Origin::Create(
             GURL(global_splash_domain_test_.value_or(kDefaultURL)))
      .GetURL()
      .spec();
}

// static
uint64_t H5vccSchemeURLLoaderFactory::GetSplashContentSizeLimit() {
  return global_splash_content_size_test_.value_or(kMaxSplashContentSize);
}

// static
std::unique_ptr<SplashCacheReader>
H5vccSchemeURLLoaderFactory::CreateSplashCacheReader(
    BrowserContext* browser_context) {
  return std::make_unique<SplashCacheReader>(browser_context, GetSplashDomain(),
                                             GetSplashContentSizeLimit());
}

// static
base::span<const uint8_t> H5vccSchemeURLLoaderFactory::GetBuiltInResource(
    const std::string& name) {
  // Built from this file only, so that the embedded data isn't duplicated.
  static const base::NoDestructor<GeneratedResourceMap> resource_map([] {
    GeneratedResourceMap map;
    LoaderEmbeddedResources::GenerateMap(map);
    return map;
  }());
  const FileContents* contents = base::FindOrNull(*resource_map, name);
  if (!contents || !contents->data || contents->size <= 0) {
    return {};
  }
  return base::span(contents->data, static_cast<size_t>(contents->size));
}

void H5vccSchemeURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<H5vccSchemeURLLoader>(
          url_request, std::move(client), browser_context_, resource_map_,
          splash_domain_, splash_content_size_limit_),
      std::move(receiver));
}

void H5vccSchemeURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<H5vccSchemeURLLoaderFactory>(browser_context_),
      std::move(receiver));
}

// static
bool H5vccSchemeURLLoaderFactory::IsCachedSplashFileEnabled() {
  if (global_cached_splash_file_enabled_test_.has_value()) {
    return *global_cached_splash_file_enabled_test_;
  }
  return base::FeatureList::IsEnabled(
      cobalt::features::kEnableCachedSplashFile);
}

void H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
    const GeneratedResourceMap* resource_map_test) {
  resource_map_test_ = resource_map_test;
}

void H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
    const std::optional<std::string>& domain) {
  global_splash_domain_test_ = domain;
}

void H5vccSchemeURLLoaderFactory::SetSplashContentSizeForTesting(
    const std::optional<int>& size) {
  global_splash_content_size_test_ = size;
}

void H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(
    const std::optional<bool>& enabled) {
  global_cached_splash_file_enabled_test_ = enabled;
}

}  // namespace content
