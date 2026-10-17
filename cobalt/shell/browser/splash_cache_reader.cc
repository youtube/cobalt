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

#include "cobalt/shell/browser/splash_cache_reader.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

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

void RecordSplashScreenFetchedState(SplashScreenFetchedState state) {
  UMA_HISTOGRAM_ENUMERATION("Cobalt.SplashScreen.FetchedFromCache", state);
}

SplashCacheReader::SplashCacheReader(BrowserContext* browser_context,
                                     const std::string& splash_domain,
                                     uint64_t max_content_size)
    : browser_context_(browser_context),
      splash_domain_(splash_domain),
      max_content_size_(max_content_size) {}

SplashCacheReader::~SplashCacheReader() = default;

void SplashCacheReader::Read(const std::string& name,
                             const std::u16string& cache_name,
                             Callback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  auto spc = StoragePartitionConfig::CreateDefault(browser_context_);
  StoragePartition* storage_partition =
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
  // The match never answers if the cache storage goes away.
  cache_storage_remote_.set_disconnect_handler(
      base::BindOnce(&SplashCacheReader::Fail, base::Unretained(this),
                     "Splash cache storage disconnected. Fallback to builtin.",
                     SplashScreenFetchedState::kErrorOnReadCache));

  auto fetch_api_request = blink::mojom::FetchAPIRequest::New();
  fetch_api_request->url = GURL(splash_domain_).Resolve(name);
  fetch_api_request->method = "GET";

  auto match_options = blink::mojom::MultiCacheQueryOptions::New();
  match_options->query_options = blink::mojom::CacheQueryOptions::New();
  match_options->cache_name = cache_name;

  cache_storage_remote_->Match(
      std::move(fetch_api_request), std::move(match_options),
      false, /* in_related_fetch_event */
      false, /* in_range_fetch_event */
      0,     // trace_id
      base::BindOnce(&SplashCacheReader::OnCacheMatched,
                     weak_factory_.GetWeakPtr(),
                     base::UTF16ToUTF8(cache_name)));
}

void SplashCacheReader::OnCacheMatched(const std::string& cache_name,
                                       blink::mojom::MatchResultPtr result) {
  // The match answered; reading its blob reports its own errors.
  cache_storage_remote_.set_disconnect_handler(base::OnceClosure());
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
    return Fail(base::StringPrintf(
                    "Did not find splash video from cache %s, reason: %d",
                    cache_name.c_str(), static_cast<int>(status)),
                state);
  }
  LOG(INFO) << "Found splash video in cache: " << cache_name;
  auto& response = result->get_response();
  if (!response->blob || response->blob->size == 0) {
    return Fail(base::StringPrintf(
                    "Splash video from %s is empty. Fallback to builtin.",
                    cache_name.c_str()),
                SplashScreenFetchedState::kErrorOnCacheEmptyContent);
  }
  if (response->blob->size > max_content_size_) {
    return Fail(base::StringPrintf(
                    "Splash video from %s is too large. Fallback to builtin.",
                    cache_name.c_str()),
                SplashScreenFetchedState::kErrorOnCacheFileOversize);
  }
  mojo::PendingRemote<blink::mojom::Blob> pending_blob_remote =
      std::move(response->blob->blob);
  blob_reader_ = std::make_unique<BlobReader>(
      std::move(pending_blob_remote), max_content_size_,
      base::BindOnce(&SplashCacheReader::OnBlobRead, weak_factory_.GetWeakPtr(),
                     response->blob->size));
}

void SplashCacheReader::OnBlobRead(uint64_t expected_size,
                                   std::vector<uint8_t> content) {
  if (content.size() != expected_size) {
    return Fail("Failed to read splash cache. Fallback to builtin.",
                SplashScreenFetchedState::kErrorOnReadCache);
  }
  Finish(SplashScreenFetchedState::kOkCache,
         std::string(content.begin(), content.end()));
}

void SplashCacheReader::Fail(const std::string& message,
                             SplashScreenFetchedState state) {
  if (state == SplashScreenFetchedState::kOkBuiltIn) {
    LOG(INFO) << message;
  } else {
    LOG(ERROR) << message;
  }
  Finish(state, std::string());
}

void SplashCacheReader::Finish(SplashScreenFetchedState state,
                               std::string content) {
  cache_storage_remote_.reset();
  // From a task of its own, as this may run in a callback of `blob_reader_`,
  // which the callback may destroy with `this`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SplashCacheReader::RunCallback,
                     weak_factory_.GetWeakPtr(), state, std::move(content)));
}

void SplashCacheReader::RunCallback(SplashScreenFetchedState state,
                                    std::string content) {
  if (callback_) {
    std::move(callback_).Run(state, std::move(content));
  }
}

}  // namespace content
