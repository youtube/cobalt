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
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/browser/splash_cache.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content {

void RecordSplashScreenFetchedState(SplashScreenFetchedState state) {
  UMA_HISTOGRAM_ENUMERATION("Cobalt.SplashScreen.FetchedFromCache", state);
}

namespace {
// Overrides kEnableCachedSplashFile for IsCachedSplashFileEnabled(), if set.
std::optional<bool> g_cached_splash_file_enabled_for_testing;
}  // namespace

bool IsCachedSplashFileEnabled() {
  if (g_cached_splash_file_enabled_for_testing.has_value()) {
    return *g_cached_splash_file_enabled_for_testing;
  }
  return base::FeatureList::IsEnabled(
      cobalt::features::kEnableCachedSplashFile);
}

void SetCachedSplashFileEnabledForTesting(std::optional<bool> enabled) {
  g_cached_splash_file_enabled_for_testing = enabled;
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

  if (!IsCachedSplashFileEnabled()) {
    // As on a cache miss, the caller falls back to the built-in resource.
    LOG(INFO) << "Cached splash assets are disabled. Skipping the cached "
              << name << ".";
    Finish(SplashScreenFetchedState::kSkippedCachedAssets, std::string());
    return;
  }

  BindSplashCacheStorage(browser_context_, splash_domain_,
                         cache_storage_remote_.BindNewPipeAndPassReceiver());
  // The match never answers if the cache storage goes away.
  cache_storage_remote_.set_disconnect_handler(
      base::BindOnce(&SplashCacheReader::Fail, base::Unretained(this),
                     "Splash cache storage disconnected. Fallback to builtin.",
                     SplashScreenFetchedState::kErrorOnReadCache));

  cache_storage_remote_->Match(
      CreateSplashRequest(splash_domain_, name),
      CreateSplashMatchOptions(cache_name), /*in_related_fetch_event=*/false,
      /*in_range_fetch_event=*/false, /*trace_id=*/0,
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
