// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_CACHE_ALIAS_SEARCH_PREFETCH_URL_LOADER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_CACHE_ALIAS_SEARCH_PREFETCH_URL_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// This class tries to fetch a prefetch response from cache, and if one is not
// available, it fetches the non-prefetch URL directly. This case is only
// triggered when cache doesn't need to be revalidated (i.e., back/forward).
class CacheAliasSearchPrefetchURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public SearchPrefetchURLLoader {
 public:
  // Creates and stores state needed to do the cache lookup.
  CacheAliasSearchPrefetchURLLoader(
      Profile* profile,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      const GURL& prefetch_url);

  ~CacheAliasSearchPrefetchURLLoader() override;

 private:
  // SearchPrefetchURLLoader:
  SearchPrefetchURLLoader::RequestHandler ServingResponseHandlerImpl(
      std::unique_ptr<SearchPrefetchURLLoader> loader) override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // Restarts the request to go directly to |resource_request_|.
  void RestartDirect();

  // The disconnect handler that is used for the fetch of the cached prefetch
  // response. This handler is not used once a fallback is started or serving is
  // started.
  void MojoDisconnectForPrefetch();

  // This handler is used for forwarding client errors and errors after a
  // fallback can not occur.
  void MojoDisconnectWithNoFallback();

  // Sets up mojo forwarding to the navigation path. Resumes
  // |network_url_loader_| calls. Serves the start of the response to the
  // navigation path. After this method is called, |this| manages its own
  // lifetime; |loader| points to |this| and can be released once the mojo
  // connection is set up.
  void SetUpForwardingClient(
      std::unique_ptr<SearchPrefetchURLLoader> loader,
      const network::ResourceRequest&,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Starts the cache only request to |prefetch_url_|.
  void StartPrefetchRequest();

  // The network URLLoader that fetches the prefetch URL and its receiver.
  mojo::Remote<network::mojom::URLLoader> network_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_receiver_{this};

  // The request that is being prefetched.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // Whether we are serving from |bdoy_content_|.
  bool can_fallback_ = true;

  // If the owner paused network activity, we need to propagate that if a
  // fallback occurs.
  bool paused_ = false;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtr<SearchPrefetchService> search_prefetch_service_;

  net::NetworkTrafficAnnotationTag network_traffic_annotation_;

  // The URL for the prefetch response stored in cache.
  GURL prefetch_url_;

  // Forwarding client receiver.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  base::WeakPtrFactory<CacheAliasSearchPrefetchURLLoader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_CACHE_ALIAS_SEARCH_PREFETCH_URL_LOADER_H_
