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

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/common/url_constants.h"
#include "cobalt/shell/embedded_resources/embedded_resources.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {
// TODO - b/456482732: remove unsafe-inline.
const char kH5vccContentSecurityPolicy[] =
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline'; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "media-src 'self' data: blob: %s:; "
    "connect-src 'self' blob: data: %s:;";
}  // namespace

class BlobReader : public blink::mojom::BlobReaderClient {
 public:
  using ContentReadyCallback = base::OnceCallback<void(std::vector<uint8_t>)>;

  BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
             ContentReadyCallback callback)
      : receiver_(this), callback_(std::move(callback)) {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    MojoResult result =
        mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle_);
    if (result != MOJO_RESULT_OK) {
      LOG(ERROR) << "Failed to create data pipe for blob reading.";
      std::move(callback_).Run({});
      return;
    }

    blob_.Bind(std::move(blob_remote));
    blob_->ReadAll(std::move(producer_handle),
                   receiver_.BindNewPipeAndPassRemote());

    watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
        base::SequencedTaskRunner::GetCurrentDefault());
    watcher_->Watch(
        consumer_handle_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&BlobReader::OnDataAvailable,
                            base::Unretained(this)));
    watcher_->Arm();
  }

  ~BlobReader() override = default;

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {
    content_.reserve(total_size);
  }

  void OnComplete(int32_t status, uint64_t data_length) override {
    if (status != net::OK) {
      LOG(ERROR) << "Blob reading failed with status: " << status;
      watcher_.reset();
      consumer_handle_.reset();
      if (callback_) {
        std::move(callback_).Run({});
      }
    }
  }

 private:
  void OnDataAvailable(MojoResult result,
                       const mojo::HandleSignalsState& state) {
    constexpr uint32_t kReadBufferSize = 64 * 1024;
    // 10MB limit for splash video to prevent memory exhaustion.
    constexpr size_t kMaxSplashVideoSize = 10 * 1024 * 1024;

    if (result != MOJO_RESULT_OK) {
      watcher_.reset();
      consumer_handle_.reset();
      if (callback_) {
        if (result != MOJO_RESULT_FAILED_PRECONDITION) {
          LOG(ERROR) << "Data pipe watcher error: " << result;
        }
        std::move(callback_).Run(std::move(content_));
      }
      return;
    }

    std::vector<uint8_t> buffer(kReadBufferSize);
    while (true) {
      uint32_t num_bytes = kReadBufferSize;
      MojoResult read_result = consumer_handle_->ReadData(
          buffer.data(), &num_bytes, MOJO_READ_DATA_FLAG_NONE);
      if (read_result == MOJO_RESULT_SHOULD_WAIT) {
        watcher_->Arm();
        return;
      }
      if (read_result != MOJO_RESULT_OK) {
        OnDataAvailable(read_result, state);
        return;
      }
      if (content_.size() + num_bytes > kMaxSplashVideoSize) {
        LOG(ERROR) << "Splash video too large, exceeding limit of "
                   << kMaxSplashVideoSize << " bytes.";
        watcher_.reset();
        consumer_handle_.reset();
        if (callback_) {
          std::move(callback_).Run({});
        }
        return;
      }
      content_.insert(content_.end(), buffer.begin(),
                      buffer.begin() + num_bytes);
    }
  }

  mojo::Remote<blink::mojom::Blob> blob_;
  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_;
  std::vector<uint8_t> content_;
  ContentReadyCallback callback_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;
};

class H5vccSchemeURLLoader : public network::mojom::URLLoader {
 public:
  H5vccSchemeURLLoader(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      BrowserContext* browser_context,
      const GeneratedResourceMap* resource_map_test,
      const std::string& splash_domain)
      : client_(std::move(client)),
        url_(request.url),
        browser_context_(browser_context),
        resource_map_test_(resource_map_test),
        splash_domain_(splash_domain),
        mime_type_("application/octet-stream") {
    client_.set_disconnect_handler(
        base::BindOnce(&H5vccSchemeURLLoader::OnClientDisconnected,
                       weak_factory_.GetWeakPtr()));
    std::string key = url_.host();
    std::string resource_key = key;

    // Get the embedded header resource
    GeneratedResourceMap resource_map;
    if (resource_map_test_) {
      resource_map = *resource_map_test_;
    } else {
      LoaderEmbeddedResources::GenerateMap(resource_map);
    }

    // Specify the built-in video if the cache is unavailable.
    // Only for webm.
    // TODO(458074360): Add fallback to static image if the device does
    // not support VP9.
    std::string fallback;
    if (net::GetValueForKeyInQuery(url_, "fallback", &fallback) &&
        (base::EndsWith(key, ".webm", base::CompareCase::SENSITIVE))) {
      LOG(INFO) << "Fallback splash: " << fallback;
      resource_key = std::move(fallback);
    }

    if (resource_map.find(resource_key) != resource_map.end()) {
      FileContents file_contents = resource_map[resource_key];
      content_ = std::string(reinterpret_cast<const char*>(file_contents.data),
                             file_contents.size);
    }

    // For html file, return from embedded resources.
    if (base::EndsWith(key, ".html", base::CompareCase::SENSITIVE)) {
      mime_type_ = "text/html";
    } else if (base::EndsWith(key, ".webm", base::CompareCase::SENSITIVE)) {
      mime_type_ = "video/webm";
      if (browser_context_) {
        ReadSplashCache(key);
        return;
      }
    }
    if (content_.empty()) {
      SendNotFoundResponse(key);
      return;
    }
    SendResponse(content_, mime_type_);
  }
  ~H5vccSchemeURLLoader() override = default;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  void ReadSplashCache(const std::string& path) {
    auto spc = content::StoragePartitionConfig::CreateDefault(browser_context_);

    content::StoragePartition* storage_partition =
        browser_context_->GetStoragePartition(spc);
    storage::mojom::CacheStorageControl* cache_storage_control =
        storage_partition->GetCacheStorageControl();
    url::Origin origin = url::Origin::Create(GURL(splash_domain_));
    blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(origin);
    storage::BucketLocator bucket_locator =
        storage::BucketLocator::ForDefaultBucket(storage_key);

    cache_storage_control->AddReceiver(
        network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
        bucket_locator, ::storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_remote_.BindNewPipeAndPassReceiver());

    auto fetch_api_request = blink::mojom::FetchAPIRequest::New();
    fetch_api_request->url = GURL(splash_domain_).Resolve(path);
    fetch_api_request->method = "GET";

    auto match_options = blink::mojom::MultiCacheQueryOptions::New();
    match_options->query_options = blink::mojom::CacheQueryOptions::New();
    std::string cache_name;
    if (net::GetValueForKeyInQuery(url_, "cache", &cache_name)) {
      match_options->cache_name = base::UTF8ToUTF16(cache_name);
    } else {
      match_options->cache_name = switches::kDefaultSplashCacheName;
    }

    std::string cache_name_utf8 =
        base::UTF16ToUTF8(match_options->cache_name.value());

    cache_storage_remote_->Match(
        std::move(fetch_api_request), std::move(match_options),
        false, /* in_related_fetch_event */
        false, /* in_range_fetch_event */
        0,     // trace_id
        base::BindOnce(&H5vccSchemeURLLoader::OnCacheMatched,
                       weak_factory_.GetWeakPtr(), cache_name_utf8));
  }

  void OnCacheMatched(const std::string& cache_name,
                      blink::mojom::MatchResultPtr result) {
    if (!result->is_response()) {
      return DisconnectCacheAndSendFallback(base::StringPrintf(
          "Failed to match splash video from cache %s, error: %d",
          cache_name.c_str(), static_cast<int>(result->get_status())));
    }
    LOG(INFO) << "Found splash video in cache: " << cache_name;
    auto& response = result->get_response();
    if (response->blob->size == 0) {
      return DisconnectCacheAndSendFallback("Splash video from " + cache_name +
                                            " is empty. Fallback to builtin.");
    }
    mojo::PendingRemote<blink::mojom::Blob> pending_blob_remote =
        std::move(response->blob->blob);
    blob_reader_ = std::make_unique<BlobReader>(
        std::move(pending_blob_remote),
        base::BindOnce(&H5vccSchemeURLLoader::SendBlobContent,
                       weak_factory_.GetWeakPtr(), response->blob->size));
  }

  void SendBlobContent(uint64_t expected_size, std::vector<uint8_t> content) {
    if (content.size() != expected_size) {
      return DisconnectCacheAndSendFallback(
          "Failed to read splash cache. Fallback to builtin.");
    }
    DisconnectCacheStorage();
    std::string cached_splash(reinterpret_cast<const char*>(content.data()),
                              content.size());
    SendResponse(cached_splash, mime_type_);
  }

 private:
  void SendNotFoundResponse(const std::string& key) {
    LOG(WARNING) << "URL: " << url_.spec() << ", host: " << key
                 << " not found.";
    SendResponse("Resource not found", "text/plain", net::HTTP_NOT_FOUND);
  }

  void DisconnectCacheStorage() { cache_storage_remote_.reset(); }

  void DisconnectCacheAndSendFallback(const std::string& message) {
    LOG(ERROR) << message;
    DisconnectCacheStorage();
    SendResponse(content_, mime_type_);
  }

  void SendResponse(const std::string& data_content,
                    const std::string& mime_type,
                    int http_status = net::HTTP_OK) {
    if (!client_) {
      LOG(ERROR) << "URLLoaderClient is not connected.";
      return;
    }

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->mime_type = mime_type;
    response_head->content_length = data_content.size();

    std::string status_line =
        "HTTP/1.1 " + std::to_string(http_status) + " " +
        net::GetHttpReasonPhrase(static_cast<net::HttpStatusCode>(http_status));
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(status_line);
    response_head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                                      mime_type);
    response_head->headers->AddHeader(net::HttpRequestHeaders::kContentLength,
                                      std::to_string(data_content.size()));
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
    options.capacity_num_bytes = data_content.size();
    if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    client_->OnReceiveResponse(std::move(response_head),
                               std::move(consumer_handle), absl::nullopt);

    uint32_t bytes_written = data_content.size();
    MojoResult result = producer_handle->WriteData(
        data_content.c_str(), &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);

    if (result != MOJO_RESULT_OK || bytes_written != data_content.size()) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = data_content.size();
    status.encoded_body_length = data_content.size();
    client_->OnComplete(status);
  }

  void OnClientDisconnected() {
    // The client is gone, so the loader is about to be destroyed. Reset the
    // remote to cancel any pending cache operations.
    cache_storage_remote_.reset();
  }

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  GURL url_;
  BrowserContext* browser_context_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_remote_;
  std::string content_;
  std::unique_ptr<BlobReader> blob_reader_;
  const GeneratedResourceMap* resource_map_test_ = nullptr;
  std::string splash_domain_;
  std::string mime_type_;
  base::WeakPtrFactory<H5vccSchemeURLLoader> weak_factory_{this};
};

std::optional<std::string>
    H5vccSchemeURLLoaderFactory::global_splash_domain_test_;
const GeneratedResourceMap* H5vccSchemeURLLoaderFactory::resource_map_test_ =
    nullptr;

H5vccSchemeURLLoaderFactory::H5vccSchemeURLLoaderFactory(
    BrowserContext* browser_context)
    : splash_domain_(
          url::Origin::Create(
              GURL(global_splash_domain_test_.value_or(switches::kDefaultURL)))
              .GetURL()
              .spec()),
      browser_context_(browser_context) {}

H5vccSchemeURLLoaderFactory::~H5vccSchemeURLLoaderFactory() = default;

void H5vccSchemeURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<H5vccSchemeURLLoader>(
          url_request, std::move(client), browser_context_, resource_map_test_,
          splash_domain_),
      std::move(receiver));
}

void H5vccSchemeURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<H5vccSchemeURLLoaderFactory>(browser_context_),
      std::move(receiver));
}

void H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
    const GeneratedResourceMap* resource_map_test) {
  resource_map_test_ = resource_map_test;
}

void H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
    const std::optional<std::string>& domain) {
  global_splash_domain_test_ = domain;
}

}  // namespace content
