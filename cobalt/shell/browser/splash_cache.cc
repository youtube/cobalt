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

#include <iterator>
#include <map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
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

namespace {

// Asynchronously deletes the cached splash assets from CacheStorage, then
// deletes itself. Self-owned so that its lifetime is independent of any
// WebContents. It's sequence-affine, and runs on the UI thread, like the
// CacheStorageControl it binds.
class SplashCacheCleaner {
 public:
  static void Start(
      mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage,
      const std::string& splash_domain,
      base::OnceClosure done_callback) {
    auto* cleaner =
        new SplashCacheCleaner(splash_domain, std::move(done_callback));
    cleaner->Clean(std::move(cache_storage));
  }

  SplashCacheCleaner(const SplashCacheCleaner&) = delete;
  SplashCacheCleaner& operator=(const SplashCacheCleaner&) = delete;

 private:
  friend class base::DeleteHelper<SplashCacheCleaner>;
  SplashCacheCleaner(const std::string& splash_domain,
                     base::OnceClosure done_callback)
      : splash_domain_(splash_domain),
        done_callback_(std::move(done_callback)) {}

  ~SplashCacheCleaner() = default;

  void Clean(mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage) {
    cache_storage_remote_.Bind(std::move(cache_storage));
    cache_storage_remote_.set_disconnect_handler(
        base::BindOnce(&SplashCacheCleaner::Finish, weak_factory_.GetWeakPtr(),
                       SplashCacheClearResult::kFailed));

    const std::u16string kCacheNames[] = {
        base::ASCIIToUTF16(kDefaultSplashCacheName),
        base::ASCIIToUTF16(kMusicSplashCacheName)};
    // TODO(b/576340659): Delete every "/splash*" entry, including
    // "splash_policy.wasm", once Cobalt reads them for the WebAssembly policy.
    // Until then, these are all the cached entries that Cobalt reads.
    const std::string kEntries[] = {"splash.webm", "splash.png"};

    // Checks for the entries with Match(), which runs as a shared operation
    // and, unlike Open(), neither takes the exclusive lock of the
    // CacheStorage nor creates missing caches. Caches are only opened to
    // delete entries that exist.
    base::RepeatingClosure barrier =
        base::BarrierClosure(std::size(kCacheNames) * std::size(kEntries),
                             base::BindOnce(&SplashCacheCleaner::OnMatchesDone,
                                            weak_factory_.GetWeakPtr()));
    for (const auto& cache_name : kCacheNames) {
      for (const auto& entry : kEntries) {
        cache_storage_remote_->Match(
            CreateSplashRequest(splash_domain_, entry),
            CreateSplashMatchOptions(cache_name),
            /*in_related_fetch_event=*/false,
            /*in_range_fetch_event=*/false,
            /*trace_id=*/0,
            base::BindOnce(&SplashCacheCleaner::OnMatched,
                           weak_factory_.GetWeakPtr(), cache_name, entry,
                           barrier));
      }
    }
  }

  void OnMatched(const std::u16string& cache_name,
                 const std::string& entry,
                 base::OnceClosure on_matched,
                 blink::mojom::MatchResultPtr result) {
    if (result->is_status()) {
      // Not found. Only these two errors are misses, like
      // H5vccSchemeURLLoader::OnCacheMatched(). Others, e.g., storage errors,
      // mean that the entry may still be cached.
      const auto status = result->get_status();
      if (status != blink::mojom::CacheStorageError::kErrorNotFound &&
          status != blink::mojom::CacheStorageError::kErrorCacheNameNotFound) {
        has_failure_ = true;
      }
    } else {
      // Found, as a response or an eager response.
      entries_to_delete_[cache_name].push_back(entry);
    }
    std::move(on_matched).Run();
  }

  void OnMatchesDone() {
    if (entries_to_delete_.empty()) {
      Finish(has_failure_ ? SplashCacheClearResult::kFailed
                          : SplashCacheClearResult::kNoCachedFiles);
      return;
    }

    LOG(INFO) << "Deleting cached splash assets from "
              << entries_to_delete_.size() << " cache(s).";
    base::RepeatingClosure barrier = base::BarrierClosure(
        entries_to_delete_.size(),
        base::BindOnce(&SplashCacheCleaner::OnDeletionsDone,
                       weak_factory_.GetWeakPtr()));
    for (const auto& [cache_name, entries] : entries_to_delete_) {
      cache_storage_remote_->Open(
          cache_name, /*trace_id=*/0,
          base::BindOnce(&SplashCacheCleaner::OnCacheOpened,
                         weak_factory_.GetWeakPtr(), entries, barrier));
    }
  }

  void OnCacheOpened(const std::vector<std::string>& entries_to_delete,
                     base::OnceClosure on_cache_cleaned,
                     blink::mojom::OpenResultPtr result) {
    if (!result->is_cache()) {
      has_failure_ = true;
      std::move(on_cache_cleaned).Run();
      return;
    }

    mojo::AssociatedRemote<blink::mojom::CacheStorageCache>& cache_remote =
        cache_remotes_.emplace_back(std::move(result->get_cache()));
    cache_remote.set_disconnect_handler(
        base::BindOnce(&SplashCacheCleaner::Finish, weak_factory_.GetWeakPtr(),
                       SplashCacheClearResult::kFailed));
    base::RepeatingClosure entry_barrier = base::BarrierClosure(
        entries_to_delete.size(), std::move(on_cache_cleaned));

    for (const auto& entry : entries_to_delete) {
      // Batch() accepts only one delete operation per call.
      auto operation = blink::mojom::BatchOperation::New();
      operation->operation_type = blink::mojom::OperationType::kDelete;
      operation->request = CreateSplashRequest(splash_domain_, entry);
      // Same options as Match(), so that it deletes the entry that was found.
      operation->match_options = CreateSplashQueryOptions();

      std::vector<blink::mojom::BatchOperationPtr> operations;
      operations.push_back(std::move(operation));

      cache_remote->Batch(
          std::move(operations), /*trace_id=*/0,
          base::BindOnce(&SplashCacheCleaner::OnCacheEntryDeleted,
                         weak_factory_.GetWeakPtr(), entry_barrier));
    }
  }

  void OnCacheEntryDeleted(base::OnceClosure on_entry_deleted,
                           blink::mojom::CacheStorageVerboseErrorPtr error) {
    if (error->value != blink::mojom::CacheStorageError::kSuccess) {
      has_failure_ = true;
    }
    std::move(on_entry_deleted).Run();
  }

  void OnDeletionsDone() {
    Finish(has_failure_ ? SplashCacheClearResult::kFailed
                        : SplashCacheClearResult::kCleared);
  }

  void Finish(SplashCacheClearResult result) {
    // Stops all callbacks and disconnect handlers, so that Finish() runs once.
    weak_factory_.InvalidateWeakPtrs();
    cache_remotes_.clear();
    cache_storage_remote_.reset();
    LOG(INFO) << "Cleared cached splash assets with result: "
              << static_cast<int>(result);
    UMA_HISTOGRAM_ENUMERATION("Cobalt.SplashScreen.ClearCachedFilesResult",
                              result);
    std::move(done_callback_).Run();
    // Deletes |this| once the current call stack, which may still be in its
    // methods, has unwound.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  const std::string splash_domain_;
  base::OnceClosure done_callback_;
  mojo::Remote<blink::mojom::CacheStorage> cache_storage_remote_;
  std::map<std::u16string, std::vector<std::string>> entries_to_delete_;
  std::vector<mojo::AssociatedRemote<blink::mojom::CacheStorageCache>>
      cache_remotes_;
  bool has_failure_ = false;
  base::WeakPtrFactory<SplashCacheCleaner> weak_factory_{this};
};

}  // namespace

void ClearCachedSplashFiles(BrowserContext* browser_context,
                            const std::string& splash_domain,
                            base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(browser_context);
  mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage;
  BindSplashCacheStorage(browser_context, splash_domain,
                         cache_storage.InitWithNewPipeAndPassReceiver());
  ClearCachedSplashFiles(std::move(cache_storage), splash_domain,
                         std::move(done_callback));
}

void ClearCachedSplashFiles(
    mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage,
    const std::string& splash_domain,
    base::OnceClosure done_callback) {
  SplashCacheCleaner::Start(std::move(cache_storage), splash_domain,
                            std::move(done_callback));
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
