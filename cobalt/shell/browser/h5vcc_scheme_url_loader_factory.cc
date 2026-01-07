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
#include "base/strings/string_util.h"
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
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/origin.h"

// TODO(b/454630524): Move below constants to command args.
namespace {
const std::string kSplashDomain = "https://www.youtube.com";
const std::string kSplashPath = "static/splash.html";
const char16_t kSplashCacheName[] = u"splash-cache";
}  // namespace

namespace content {

namespace {
// TODO - b/456482732: remove unsafe-inline.
const char kH5vccContentSecurityPolicy[] =
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline'; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "media-src 'self' data: blob:; "
    "connect-src 'self' blob: data:;";
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
                        uint64_t expected_content_size) override {}

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
    constexpr uint32_t kReadBufferSize = 256;
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

    while (true) {
      uint8_t buffer[kReadBufferSize];
      size_t num_bytes = sizeof(buffer);
      MojoResult read_result = consumer_handle_->ReadData(
          MOJO_READ_DATA_FLAG_NONE, buffer, num_bytes);
      if (read_result == MOJO_RESULT_SHOULD_WAIT) {
        watcher_->Arm();
        return;
      }
      if (read_result != MOJO_RESULT_OK) {
        OnDataAvailable(read_result, state);
        return;
      }
      content_.insert(content_.end(), buffer, buffer + num_bytes);
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
      const GeneratedResourceMap* resource_map_test)
      : client_(std::move(client)),
        url_(request.url),
        browser_context_(browser_context),
        resource_map_test_(resource_map_test) {
    client_.set_disconnect_handler(
        base::BindOnce(&H5vccSchemeURLLoader::OnClientDisconnected,
                       weak_factory_.GetWeakPtr()));
    std::string key = url_.host();

    // Get the embedded header resource
    GeneratedResourceMap resource_map;
    if (resource_map_test_) {
      resource_map = *resource_map_test_;
    } else {
      LoaderEmbeddedResources::GenerateMap(resource_map);
    }

    if (resource_map.find(key) == resource_map.end()) {
      LOG(WARNING) << "URL: " << url_.spec() << ", host: " << key
                   << " not found.";
      SendResponse("Resource not found", "text/plain", net::HTTP_NOT_FOUND);
      return;
    }

    FileContents file_contents = resource_map[key];
    std::string mime_type = "application/octet-stream";
    content_ = std::string(reinterpret_cast<const char*>(file_contents.data),
                           file_contents.size);

    if (base::EndsWith(key, ".html", base::CompareCase::SENSITIVE)) {
      if (browser_context_) {
        ReadSplashCache();
        return;
      }
      mime_type = "text/html";
    } else if (base::EndsWith(key, ".webm", base::CompareCase::SENSITIVE)) {
      // TODO(b/454630524): Support cached webm files.
      mime_type = "video/webm";
    }
    SendResponse(content_, mime_type);
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
void ReadSplashCache() {
    auto spc = content::StoragePartitionConfig::CreateDefault(browser_context_);

    content::StoragePartition* storage_partition =
        browser_context_->GetStoragePartition(spc);
    storage::mojom::CacheStorageControl* cache_storage_control =
        storage_partition->GetCacheStorageControl();
    url::Origin origin = url::Origin::Create(GURL(kSplashDomain));
    blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(origin);
    storage::BucketLocator bucket_locator =
        storage::BucketLocator::ForDefaultBucket(storage_key);

    cache_storage_control->AddReceiver(
        network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
        network::DocumentIsolationPolicy(), mojo::NullRemote(),
        bucket_locator, storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_remote_.BindNewPipeAndPassReceiver());

    auto fetch_api_request = blink::mojom::FetchAPIRequest::New();
    fetch_api_request->url = GURL(kSplashDomain).Resolve(kSplashPath);
    fetch_api_request->method = "GET";

    auto match_options = blink::mojom::MultiCacheQueryOptions::New();
    match_options->query_options = blink::mojom::CacheQueryOptions::New();
    match_options->cache_name = kSplashCacheName;

    cache_storage_remote_->Match(
        std::move(fetch_api_request), std::move(match_options),
        false, /* in_related_fetch_event */
        false, /* in_range_fetch_event */
        0,     // trace_id
        base::BindOnce(&H5vccSchemeURLLoader::OnCacheMatched,
                       weak_factory_.GetWeakPtr()));
  }
  void OnCacheMatched(blink::mojom::MatchResultPtr result) {
    if (!result->is_response()) {
      LOG(ERROR) << "Failed to match cache for splash video"
                 << ", error: " << result->get_status();
      SendResponse(content_, "text/html");
      return;
    }
    LOG(INFO) << "Found splash video in cache.";
    auto& response = result->get_response();
    if (response->blob->size == 0) {
      LOG(ERROR) << "Splash video cache is empty. Fallback to builtin.";
      SendResponse(content_, "text/html");
      return;
    }
    mojo::PendingRemote<blink::mojom::Blob> pending_blob_remote =
        std::move(response->blob->blob);
    blob_reader_ = std::make_unique<BlobReader>(
        std::move(pending_blob_remote),
        base::BindOnce(&H5vccSchemeURLLoader::SendBlobContent,
                       weak_factory_.GetWeakPtr(), "text/html",
                       response->blob->size));
  }

  void SendBlobContent(const std::string& mime_type,
                       uint64_t expected_size,
                       std::vector<uint8_t> content) {
    if (content.size() != expected_size) {
      LOG(ERROR) << "Failed to read splash cache. Fallback to builtin.";
      SendResponse(content_, mime_type);
      return;
    }
    std::string cached_html(reinterpret_cast<const char*>(content.data()),
                            content.size());
    SendResponse(cached_html, mime_type);
  }
 private:
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
    response_head->headers->AddHeader("Content-Security-Policy",
                                      kH5vccContentSecurityPolicy);
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
                               std::move(consumer_handle), std::nullopt);

    size_t bytes_written = 0;
    MojoResult result = producer_handle->WriteData(
        base::as_byte_span(data_content), MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
        bytes_written);

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
  base::WeakPtrFactory<H5vccSchemeURLLoader> weak_factory_{this};
};

H5vccSchemeURLLoaderFactory::H5vccSchemeURLLoaderFactory(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

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
          url_request, std::move(client), browser_context_, resource_map_test_),
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

}  // namespace content
