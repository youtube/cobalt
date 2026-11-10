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

#ifndef COBALT_SHELL_BROWSER_SPLASH_CACHE_H_
#define COBALT_SHELL_BROWSER_SPLASH_CACHE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

// Helpers to access the splash assets that the WebApp caches in the
// CacheStorage of the splash domain, e.g., "splash.webm" in the "default"
// cache.

namespace content {

class BrowserContext;

// Binds |receiver| to the CacheStorage of the origin of |splash_domain|, in the
// default StoragePartition of |browser_context|.
void BindSplashCacheStorage(
    BrowserContext* browser_context,
    const std::string& splash_domain,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

// Returns the request for the cached splash asset |entry|, e.g.,
// "splash.webm", resolved against the origin of |splash_domain|.
blink::mojom::FetchAPIRequestPtr CreateSplashRequest(
    const std::string& splash_domain,
    const std::string& entry);

// Returns the options to query a splash asset in a cache. Operations on the
// same asset, e.g., matching and deleting it, should use the same options, so
// that they find the same entries.
blink::mojom::CacheQueryOptionsPtr CreateSplashQueryOptions();

// Returns the options to match a splash asset in the cache |cache_name|, with
// CreateSplashQueryOptions().
blink::mojom::MultiCacheQueryOptionsPtr CreateSplashMatchOptions(
    const std::u16string& cache_name);

// Reads the content of a cached splash asset from its blob, up to
// |max_content_size| bytes, then runs |callback| with the content, which is
// empty on errors, e.g., if it's too large. The owner must keep it alive until
// |callback| runs. It's sequence-affine, and runs |callback| on the sequence
// where it's created.
class BlobReader : public blink::mojom::BlobReaderClient {
 public:
  using ContentReadyCallback = base::OnceCallback<void(std::vector<uint8_t>)>;

  BlobReader(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
             uint64_t max_content_size,
             ContentReadyCallback callback);

  BlobReader(const BlobReader&) = delete;
  BlobReader& operator=(const BlobReader&) = delete;

  ~BlobReader() override;

  // blink::mojom::BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override;

 private:
  void OnDataAvailable(MojoResult result,
                       const mojo::HandleSignalsState& state);
  // Finishes reading when the data pipe is closed, normally or on errors.
  void OnPipeClosed(MojoResult result);
  void Abort();

  mojo::Remote<blink::mojom::Blob> blob_;
  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_;
  std::vector<uint8_t> content_;
  uint64_t max_content_size_;
  ContentReadyCallback callback_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_CACHE_H_
