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

#include "cobalt/shell/browser/splash/splash_cache_reader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

// Collects the bytes of a blob from a data pipe, up to a size limit.
class SplashCacheReader::Drainer : public mojo::DataPipeDrainer::Client {
 public:
  using DoneCallback = base::OnceCallback<void(std::vector<uint8_t>)>;

  Drainer(mojo::ScopedDataPipeConsumerHandle source,
          uint64_t max_size,
          DoneCallback done)
      : max_size_(max_size), done_(std::move(done)) {
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(this, std::move(source));
  }

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    if (overflowed_ || content_.size() + data.size() > max_size_) {
      overflowed_ = true;
      return;
    }
    content_.insert(content_.end(), data.begin(), data.end());
  }

  void OnDataComplete() override {
    if (overflowed_) {
      content_.clear();
    }
    std::move(done_).Run(std::move(content_));
  }

 private:
  const uint64_t max_size_;
  bool overflowed_ = false;
  std::vector<uint8_t> content_;
  DoneCallback done_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
};

SplashCacheReader::SplashCacheReader(BrowserContext* browser_context,
                                     const GURL& splash_origin,
                                     std::u16string cache_name)
    : browser_context_(browser_context),
      splash_origin_(splash_origin),
      cache_name_(std::move(cache_name)) {}

SplashCacheReader::~SplashCacheReader() = default;

// static
const char* SplashCacheReader::ErrorToString(Error error) {
  switch (error) {
    case Error::kNotFound:
      return "not found";
    case Error::kEmpty:
      return "empty";
    case Error::kTooLarge:
      return "too large";
    case Error::kReadFailed:
      return "read failed";
  }
  return "unknown";
}

void SplashCacheReader::Read(const std::string& key,
                             uint64_t max_size,
                             ReadCallback callback) {
  DCHECK(!callback_) << "One read at a time";
  callback_ = std::move(callback);

  if (!cache_storage_) {
    // Same as H5vccSchemeURLLoader::ReadSplashCache().
    StoragePartition* storage_partition = browser_context_->GetStoragePartition(
        StoragePartitionConfig::CreateDefault(browser_context_));
    storage::BucketLocator bucket_locator =
        storage::BucketLocator::ForDefaultBucket(
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(splash_origin_)));
    storage_partition->GetCacheStorageControl()->AddReceiver(
        network::CrossOriginEmbedderPolicy(), mojo::NullRemote(),
        network::DocumentIsolationPolicy(), mojo::NullRemote(), bucket_locator,
        storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_.BindNewPipeAndPassReceiver());
    cache_storage_.set_disconnect_handler(
        base::BindOnce(&SplashCacheReader::Finish, base::Unretained(this),
                       base::unexpected(Error::kReadFailed)));
  }

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = splash_origin_.Resolve(key);
  request->method = "GET";
  auto options = blink::mojom::MultiCacheQueryOptions::New();
  options->query_options = blink::mojom::CacheQueryOptions::New();
  options->cache_name = cache_name_;
  cache_storage_->Match(std::move(request), std::move(options),
                        /*in_related_fetch_event=*/false,
                        /*in_range_fetch_event=*/false,
                        /*trace_id=*/0,
                        base::BindOnce(&SplashCacheReader::OnMatched,
                                       weak_factory_.GetWeakPtr(), max_size));
}

void SplashCacheReader::OnMatched(uint64_t max_size,
                                  blink::mojom::MatchResultPtr result) {
  if (!result->is_response()) {
    const auto status = result->get_status();
    if (status == blink::mojom::CacheStorageError::kErrorCacheNameNotFound ||
        status == blink::mojom::CacheStorageError::kErrorNotFound) {
      return Finish(base::unexpected(Error::kNotFound));
    }
    return Finish(base::unexpected(Error::kReadFailed));
  }
  auto& response = result->get_response();
  if (!response->blob || response->blob->size == 0) {
    return Finish(base::unexpected(Error::kEmpty));
  }
  if (response->blob->size > max_size) {
    return Finish(base::unexpected(Error::kTooLarge));
  }

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    return Finish(base::unexpected(Error::kReadFailed));
  }
  const uint64_t expected_size = response->blob->size;
  blob_.reset();
  blob_.Bind(std::move(response->blob->blob));
  blob_->ReadAll(std::move(producer), mojo::NullRemote());
  drainer_ = std::make_unique<Drainer>(
      std::move(consumer), max_size,
      base::BindOnce(&SplashCacheReader::OnDrained, weak_factory_.GetWeakPtr(),
                     expected_size));
}

void SplashCacheReader::OnDrained(uint64_t expected_size,
                                  std::vector<uint8_t> content) {
  if (content.size() != expected_size) {
    return Finish(base::unexpected(Error::kReadFailed));
  }
  Finish(std::move(content));
}

void SplashCacheReader::Finish(
    base::expected<std::vector<uint8_t>, Error> result) {
  // The drainer may be the caller; destroy it asynchronously.
  if (drainer_) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(drainer_));
  }
  blob_.reset();
  if (callback_) {
    std::move(callback_).Run(std::move(result));
  }
}

}  // namespace content
