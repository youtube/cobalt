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
#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/browser/splash_asset_list.h"
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
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using switches::kDefaultSplashCacheName;
using switches::kDefaultURL;
using switches::kMaxSplashContentSize;

namespace {
const char kMimeTypeApplicationJson[] = "application/json";
const char kMimeTypeApplicationOctetStream[] = "application/octet-stream";
const char kMimeTypeImagePng[] = "image/png";
const char kMimeTypeTextHtml[] = "text/html";
const char kMimeTypeTextPlain[] = "text/plain";
const char kMimeTypeVideoWebM[] = "video/webm";

// TODO - b/456482732: remove unsafe-inline.
// The first %s is kWasmUnsafeEval, or nothing.
const char kH5vccContentSecurityPolicy[] =
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline'%s; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "media-src 'self' data: blob: %s:; "
    "connect-src 'self' blob: data: %s:;";

// With kSplashWasmPolicy, lets splash.html compile its WebAssembly policy.
const char kWasmUnsafeEval[] = " 'wasm-unsafe-eval'";

// With kSplashWasmPolicy, the list of the splash screen assets that
// splash.html's WebAssembly policy chooses from (SplashAssetList).
const char kSplashAssetListName[] = "splash_assets.json";

// With kSplashWasmPolicy, the query params of splash.html's request for the
// asset that its WebAssembly policy chose:
// - "source=builtin" for a built-in asset, which is then served without
//   reading the cache, as the policy found no cached asset to show instead;
// - the policy's outcome, e.g., "policy=builtin" (see SplashWasmPolicyOutcome);
// - when the policy chose, in ms since the page's time origin, e.g.,
//   "policy_ms=250".
const char kSourceParam[] = "source";
const char kSourceBuiltIn[] = "builtin";
const char kPolicyParam[] = "policy";
const char kPolicyTimeParam[] = "policy_ms";

// Records the outcome of splash.html's WebAssembly policy and, if it chose an
// asset, when, if `url`, the page's request for the asset that it shows,
// reports them.
void RecordSplashWasmPolicyMetrics(const GURL& url) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, kPolicyParam, &value)) {
    return;
  }
  SplashWasmPolicyOutcome outcome;
  if (value == "builtin") {
    outcome = SplashWasmPolicyOutcome::kChoseBuiltIn;
  } else if (value == "cached") {
    outcome = SplashWasmPolicyOutcome::kChoseCached;
  } else if (value == "list_unavailable") {
    outcome = SplashWasmPolicyOutcome::kListUnavailable;
  } else if (value == "failed") {
    outcome = SplashWasmPolicyOutcome::kFailed;
  } else {
    LOG(WARNING) << "Unknown splash screen policy outcome: " << value;
    return;
  }
  base::UmaHistogramEnumeration("Cobalt.SplashScreen.WasmPolicy.Outcome",
                                outcome);

  int64_t decision_ms = 0;
  if ((outcome == SplashWasmPolicyOutcome::kChoseBuiltIn ||
       outcome == SplashWasmPolicyOutcome::kChoseCached) &&
      net::GetValueForKeyInQuery(url, kPolicyTimeParam, &value) &&
      base::StringToInt64(value, &decision_ms) && decision_ms >= 0) {
    base::UmaHistogramTimes("Cobalt.SplashScreen.WasmPolicy.DecisionTime",
                            base::Milliseconds(decision_ms));
  }
}
}  // namespace

class BlobReader : public blink::mojom::BlobReaderClient {
 public:
  using ContentReadyCallback = base::OnceCallback<void(std::vector<uint8_t>)>;

  BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
             uint64_t max_content_size,
             ContentReadyCallback callback)
      : receiver_(this),
        max_content_size_(max_content_size),
        callback_(std::move(callback)) {
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
    watcher_->ArmOrNotify();
  }

  ~BlobReader() override = default;

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {
    if (total_size > max_content_size_) {
      return Abort();
    }
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
      size_t num_bytes = 0;
      MojoResult read_result = consumer_handle_->ReadData(
          MOJO_READ_DATA_FLAG_NONE, base::span(buffer), num_bytes);
      if (read_result == MOJO_RESULT_SHOULD_WAIT) {
        watcher_->ArmOrNotify();
        return;
      }
      if (read_result != MOJO_RESULT_OK) {
        OnDataAvailable(read_result, state);
        return;
      }
      if (content_.size() + num_bytes > max_content_size_) {
        return Abort();
      }
      content_.insert(content_.end(), buffer, buffer + num_bytes);
    }
  }

  void Abort() {
    LOG(ERROR) << "Splash content too large, exceeding limit of "
               << max_content_size_ << " bytes.";
    watcher_.reset();
    consumer_handle_.reset();
    if (callback_) {
      std::move(callback_).Run({});
    }
  }

  mojo::Remote<blink::mojom::Blob> blob_;
  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_;
  std::vector<uint8_t> content_;
  uint64_t max_content_size_;
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
      const GeneratedResourceMap& resource_map,
      const std::string& splash_domain,
      uint64_t splash_content_size_limit)
      : client_(std::move(client)),
        url_(request.url),
        browser_context_(browser_context),
        splash_domain_(splash_domain),
        splash_content_size_limit_(splash_content_size_limit),
        mime_type_(kMimeTypeApplicationOctetStream),
        is_splash_wasm_policy_enabled_(
            base::FeatureList::IsEnabled(cobalt::features::kSplashWasmPolicy)) {
    client_.set_disconnect_handler(
        base::BindOnce(&H5vccSchemeURLLoader::OnClientDisconnected,
                       weak_factory_.GetWeakPtr()));
    std::string key = url_.host();
    // The key to query the resource map. It is usually the same as the host,
    // but can be overridden by the "fallback" query param for png/webm
    // resources, so that callers can specify a different file for fallback.
    std::string resource_key = key;

    if (is_splash_wasm_policy_enabled_ && key == kSplashAssetListName) {
      SendSplashAssetList();
      return;
    }

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
    const bool is_cached_splash_file_enabled =
        H5vccSchemeURLLoaderFactory::IsCachedSplashFileEnabled();
    // Whether splash.html's WebAssembly policy chose this built-in asset, as
    // it found no cached asset to show instead, so that the cache isn't read.
    bool is_built_in_policy_choice = false;
    if (is_splash_asset && is_splash_wasm_policy_enabled_) {
      RecordSplashWasmPolicyMetrics(url_);
      std::string source;
      is_built_in_policy_choice =
          net::GetValueForKeyInQuery(url_, kSourceParam, &source) &&
          source == kSourceBuiltIn;
    }
    // Whether to read the splash asset that the WebApp cached.
    const bool supports_splash_caching = is_splash_asset &&
                                         is_cached_splash_file_enabled &&
                                         !is_built_in_policy_choice;

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
      ReadSplashCache(key);
      return;
    }

    if (content_.empty()) {
      SendNotFoundResponse(key);
      return;
    }

    if (is_built_in_policy_choice && is_cached_splash_file_enabled) {
      // As on a cache miss, which the policy's list of the cached assets
      // already showed.
      VLOG(1) << "Using built-in " << resource_key
              << ", as the splash screen's policy chose.";
      SendResponse(SplashScreenFetchedState::kOkBuiltIn);
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
        network::DocumentIsolationPolicy(), mojo::NullRemote(), bucket_locator,
        storage::mojom::CacheStorageOwner::kCacheAPI,
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
      match_options->cache_name = kDefaultSplashCacheName;
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
      const auto status = result->get_status();
      SplashScreenFetchedState state =
          SplashScreenFetchedState::kErrorOnReadCache;
      // If the cache entry is not found, or the splash cache is not found in
      // the cache entry, it's not an error of reading cache, but just a cache
      // miss.
      if (status == blink::mojom::CacheStorageError::kErrorCacheNameNotFound ||
          status == blink::mojom::CacheStorageError::kErrorNotFound) {
        state = SplashScreenFetchedState::kOkBuiltIn;
      }
      return DisconnectCacheAndSendFallback(
          base::StringPrintf(
              "Did not find splash video from cache %s, reason: %d",
              cache_name.c_str(), static_cast<int>(status)),
          state);
    }
    LOG(INFO) << "Found splash video in cache: " << cache_name;
    auto& response = result->get_response();
    if (!response->blob || response->blob->size == 0) {
      return DisconnectCacheAndSendFallback(
          base::StringPrintf(
              "Splash video from %s is empty. Fallback to builtin.",
              cache_name.c_str()),
          SplashScreenFetchedState::kErrorOnCacheEmptyContent);
    }
    if (response->blob->size > splash_content_size_limit_) {
      return DisconnectCacheAndSendFallback(
          base::StringPrintf(
              "Splash video from %s is too large. Fallback to builtin.",
              cache_name.c_str()),
          SplashScreenFetchedState::kErrorOnCacheFileOversize);
    }
    mojo::PendingRemote<blink::mojom::Blob> pending_blob_remote =
        std::move(response->blob->blob);
    blob_reader_ = std::make_unique<BlobReader>(
        std::move(pending_blob_remote), splash_content_size_limit_,
        base::BindOnce(&H5vccSchemeURLLoader::SendBlobContent,
                       weak_factory_.GetWeakPtr(), response->blob->size));
  }

  void SendBlobContent(uint64_t expected_size, std::vector<uint8_t> content) {
    if (content.size() != expected_size) {
      return DisconnectCacheAndSendFallback(
          "Failed to read splash cache. Fallback to builtin.",
          SplashScreenFetchedState::kErrorOnReadCache);
    }
    DisconnectCacheStorage();
    content_ = std::string(reinterpret_cast<const char*>(content.data()),
                           content.size());
    SendResponse(SplashScreenFetchedState::kOkCache);
  }

 private:
  // Sends the list of the splash screen assets that splash.html's WebAssembly
  // policy chooses from, once it's ready.
  void SendSplashAssetList() {
    if (!browser_context_) {
      // There's no cache to list, e.g., in unit tests.
      OnSplashAssetListReady(std::nullopt);
      return;
    }
    SplashAssetList::GetOrCreateForBrowserContext(browser_context_)
        ->Get(base::BindOnce(&H5vccSchemeURLLoader::OnSplashAssetListReady,
                             weak_factory_.GetWeakPtr()));
  }

  void OnSplashAssetListReady(std::optional<std::string> json) {
    if (!json) {
      // The page then makes its own choice, which reads the cache, so that a
      // cached asset isn't missed.
      content_ = "Splash asset list unavailable";
      mime_type_ = kMimeTypeTextPlain;
      SendResponse(net::HTTP_SERVICE_UNAVAILABLE);
      return;
    }
    content_ = std::move(*json);
    mime_type_ = kMimeTypeApplicationJson;
    SendResponse();
  }

  void SendNotFoundResponse(const std::string& key) {
    LOG(WARNING) << "URL: " << url_.spec() << ", host: " << key
                 << " not found.";
    content_ = "Resource not found";
    mime_type_ = kMimeTypeTextPlain;
    SendResponse(SplashScreenFetchedState::kErrorOnResourceNotFound,
                 net::HTTP_NOT_FOUND);
  }

  void DisconnectCacheStorage() { cache_storage_remote_.reset(); }

  void DisconnectCacheAndSendFallback(const std::string& message,
                                      SplashScreenFetchedState state) {
    if (state == SplashScreenFetchedState::kOkBuiltIn) {
      LOG(INFO) << message;
    } else {
      LOG(ERROR) << message;
    }
    DisconnectCacheStorage();
    SendResponse(state);
  }

  void SendResponse(SplashScreenFetchedState state,
                    int http_status = net::HTTP_OK) {
    UMA_HISTOGRAM_ENUMERATION("Cobalt.SplashScreen.FetchedFromCache", state);
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
        base::StringPrintf(
            kH5vccContentSecurityPolicy,
            is_splash_wasm_policy_enabled_ ? kWasmUnsafeEval : "",
            kH5vccEmbeddedScheme, kH5vccEmbeddedScheme));
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
    // The client is gone, so the loader is about to be destroyed. Reset the
    // remote to cancel any pending cache operations.
    cache_storage_remote_.reset();
  }

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  GURL url_;
  BrowserContext* browser_context_;
  std::string splash_domain_;
  uint64_t splash_content_size_limit_;
  std::string mime_type_;
  // kSplashWasmPolicy.
  const bool is_splash_wasm_policy_enabled_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_remote_;
  std::string content_;
  std::unique_ptr<BlobReader> blob_reader_;
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
      splash_content_size_limit_(
          global_splash_content_size_test_.value_or(kMaxSplashContentSize)),
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
