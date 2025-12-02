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

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "cobalt/shell/embedded_resources/embedded_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "cobalt/shell/browser/shell_browser_context.h"

namespace content {

// TODO - b/456482732: remove unsafe-inline.
const char kH5vccContentSecurityPolicy[] =
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline'; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "media-src 'self' data: blob:; "
    "connect-src 'self' blob: data:;";

class H5vccSchemeURLLoader : public network::mojom::URLLoader {
 public:
  H5vccSchemeURLLoader(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      ShellBrowserContext* browser_context)
      : client_(std::move(client)),
        url_(request.url),
        browser_context_(browser_context) {
    std::string key = url_.host();

    // Get the embedded header resource
    GeneratedResourceMap resource_map;
    LoaderEmbeddedResources::GenerateMap(resource_map);

    if (resource_map.find(key) == resource_map.end()) {
      LOG(WARNING) << "URL: " << url_.spec() << ", host: " << key
                   << " not found.";
      SendResponse("Resource not found", "text/plain", net::HTTP_NOT_FOUND);
      return;
    }

    FileContents file_contents = resource_map[key];
    std::string mime_type = "application/octet-stream";

    if (base::EndsWith(key, ".html", base::CompareCase::SENSITIVE)) {
      mime_type = "text/html";
    } else if (base::EndsWith(key, ".webm", base::CompareCase::SENSITIVE)) {
      mime_type = "video/webm";
    }
    std::string content(reinterpret_cast<const char*>(file_contents.data),
                        file_contents.size);
    
    // base::ThreadPool::PostTaskAndReplyWithResult(
    //         FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
    //         base::BindOnce(&H5vccSchemeURLLoader::ReadSplashCache,
    //                        weak_factory_.GetWeakPtr()),
    //         base::BindOnce(&H5vccSchemeURLLoader::OnSplashVideoFileRead,
    //                        weak_factory_.GetWeakPtr()));
    //SendResponse(content, mime_type);
    ReadSplashCache();
    return;

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

  void ReadSplashCache() {
    LOG(INFO) << "lxn:::open partition";
    content::StoragePartition* storage_partition = browser_context_->GetStoragePartitionForUrl(
        GURL("https://lxn-test.uc.r.appspot.com/"));
    ::storage::mojom::CacheStorageControl* cache_storage_control =
        storage_partition->GetCacheStorageControl();
    url::Origin origin =
        url::Origin::Create(GURL("https://lxn-test.uc.r.appspot.com/"));
    blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(origin);
    ::storage::BucketLocator bucket_locator =
        ::storage::BucketLocator::ForDefaultBucket(storage_key);

    cache_storage_control->AddReceiver(
        ::network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
        bucket_locator, ::storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_remote_.BindNewPipeAndPassReceiver());
    const char16_t kSplashCacheName[] = u"splash-cache-v1";
    LOG(INFO) << "lxn:::ready to open cache with origin: " << origin;
    cache_storage_remote_->Has(
        kSplashCacheName, 0,  // trace-id
        base::BindOnce(&H5vccSchemeURLLoader::OnCacheOpened,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCacheOpened(
    blink::mojom::CacheStorageError result) {
    if (result == blink::mojom::CacheStorageError::kSuccess) {
      LOG(INFO) << "lxn:::Cache opened for h5vcc://splash";
      // TODO: Add logic to read from the cache and send its content.
      SendResponse("Splash screen from cache!", "text/plain");
    } else {
      LOG(ERROR) << "lxn:::Failed to open cache: " << result;
      SendResponse("Error opening splash cache", "text/plain",
                   net::HTTP_INTERNAL_SERVER_ERROR);
    }
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
    // We should support HTTP range requests.
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
  ShellBrowserContext* browser_context_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_remote_;
  base::WeakPtrFactory<H5vccSchemeURLLoader> weak_factory_{this};

};

H5vccSchemeURLLoaderFactory::H5vccSchemeURLLoaderFactory(
    ShellBrowserContext* browser_context)
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
      std::make_unique<H5vccSchemeURLLoader>(url_request, std::move(client),
                                             browser_context_),
      std::move(receiver));
}

void H5vccSchemeURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<H5vccSchemeURLLoaderFactory>(browser_context_),
      std::move(receiver));
}

}  // namespace content
