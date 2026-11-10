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

#include "cobalt/shell/browser/splash_cache.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

void BindSplashCacheStorage(
    BrowserContext* browser_context,
    const std::string& splash_domain,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK(browser_context);
  auto spc = content::StoragePartitionConfig::CreateDefault(browser_context);

  content::StoragePartition* storage_partition =
      browser_context->GetStoragePartition(spc);
  storage::mojom::CacheStorageControl* cache_storage_control =
      storage_partition->GetCacheStorageControl();
  url::Origin origin = url::Origin::Create(GURL(splash_domain));
  blink::StorageKey storage_key = blink::StorageKey::CreateFirstParty(origin);
  storage::BucketLocator bucket_locator =
      storage::BucketLocator::ForDefaultBucket(storage_key);

  cache_storage_control->AddReceiver(
      network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
      network::DocumentIsolationPolicy(), mojo::NullRemote(), bucket_locator,
      storage::mojom::CacheStorageOwner::kCacheAPI, std::move(receiver));
}

blink::mojom::FetchAPIRequestPtr CreateSplashRequest(
    const std::string& splash_domain,
    const std::string& entry) {
  auto fetch_api_request = blink::mojom::FetchAPIRequest::New();
  // Resolves against the origin, like the CacheStorage that
  // BindSplashCacheStorage() binds, where the WebApp caches the asset.
  fetch_api_request->url =
      url::Origin::Create(GURL(splash_domain)).GetURL().Resolve(entry);
  fetch_api_request->method = "GET";
  return fetch_api_request;
}

blink::mojom::CacheQueryOptionsPtr CreateSplashQueryOptions() {
  return blink::mojom::CacheQueryOptions::New();
}

blink::mojom::MultiCacheQueryOptionsPtr CreateSplashMatchOptions(
    const std::u16string& cache_name) {
  auto match_options = blink::mojom::MultiCacheQueryOptions::New();
  match_options->query_options = CreateSplashQueryOptions();
  match_options->cache_name = cache_name;
  return match_options;
}

BlobReader::BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
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
  watcher_->Watch(consumer_handle_.get(),
                  MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                  MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                  base::BindRepeating(&BlobReader::OnDataAvailable,
                                      base::Unretained(this)));
  watcher_->ArmOrNotify();
}

BlobReader::~BlobReader() = default;

void BlobReader::OnCalculatedSize(uint64_t total_size,
                                  uint64_t expected_content_size) {
  if (total_size > max_content_size_) {
    Abort();
    return;
  }
  content_.reserve(total_size);
}

void BlobReader::OnComplete(int32_t status, uint64_t data_length) {
  if (status != net::OK) {
    LOG(ERROR) << "Blob reading failed with status: " << status;
    watcher_.reset();
    consumer_handle_.reset();
    if (callback_) {
      std::move(callback_).Run({});
    }
  }
}

void BlobReader::OnDataAvailable(MojoResult result,
                                 const mojo::HandleSignalsState&) {
  constexpr uint32_t kReadBufferSize = 64 * 1024;
  if (result != MOJO_RESULT_OK) {
    OnPipeClosed(result);
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
      OnPipeClosed(read_result);
      return;
    }
    if (content_.size() + num_bytes > max_content_size_) {
      Abort();
      return;
    }
    content_.insert(content_.end(), buffer, buffer + num_bytes);
  }
}

void BlobReader::OnPipeClosed(MojoResult result) {
  watcher_.reset();
  consumer_handle_.reset();
  if (callback_) {
    if (result != MOJO_RESULT_FAILED_PRECONDITION) {
      LOG(ERROR) << "Data pipe watcher error: " << result;
    }
    std::move(callback_).Run(std::move(content_));
  }
}

void BlobReader::Abort() {
  LOG(ERROR) << "Splash content too large, exceeding limit of "
             << max_content_size_ << " bytes.";
  watcher_.reset();
  consumer_handle_.reset();
  if (callback_) {
    std::move(callback_).Run({});
  }
}

}  // namespace content
