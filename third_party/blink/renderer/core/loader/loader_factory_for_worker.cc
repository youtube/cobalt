// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/loader_factory_for_worker.h"

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle_factory.mojom-blink.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"

namespace blink {

void LoaderFactoryForWorker::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  LoaderFactory::Trace(visitor);
}

std::unique_ptr<URLLoader> LoaderFactoryForWorker::CreateURLLoader(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper) {
  WrappedResourceRequest wrapped(request);

  mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
      url_loader_factory;
  if (options.url_loader_factory) {
    mojo::Remote<network::mojom::blink::URLLoaderFactory>
        url_loader_factory_remote(std::move(options.url_loader_factory->data));
    url_loader_factory_remote->Clone(
        url_loader_factory.InitWithNewPipeAndPassReceiver());
  }
  // Resolve any blob: URLs that haven't been resolved yet. The XHR and
  // fetch() API implementations resolve blob URLs earlier because there can
  // be arbitrarily long delays between creating requests with those APIs and
  // actually creating the URL loader here. Other subresource loading will
  // immediately create the URL loader so resolving those blob URLs here is
  // simplest.
  if (request.Url().ProtocolIs("blob") && !url_loader_factory) {
    global_scope_->GetPublicURLManager().Resolve(
        request.Url(), url_loader_factory.InitWithNewPipeAndPassReceiver());
  }

  // KeepAlive is not yet supported in web workers.
  mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle =
      mojo::NullRemote();

  if (url_loader_factory) {
    return web_context_->WrapURLLoaderFactory(std::move(url_loader_factory))
        ->CreateURLLoader(wrapped, freezable_task_runner,
                          unfreezable_task_runner, std::move(keep_alive_handle),
                          back_forward_cache_loader_helper);
  }

  // If |global_scope_| is a service worker, use |script_loader_factory_| for
  // the following request contexts.
  // - SERVICE_WORKER for a classic main script, a module main script, or a
  //   module imported script.
  // - SCRIPT for a classic imported script.
  //
  // Other workers (dedicated workers, shared workers, and worklets) don't have
  // a loader specific to script loading.
  if (global_scope_->IsServiceWorkerGlobalScope()) {
    if (request.GetRequestContext() ==
            mojom::blink::RequestContextType::SERVICE_WORKER ||
        request.GetRequestContext() ==
            mojom::blink::RequestContextType::SCRIPT) {
      // GetScriptLoaderFactory() may return nullptr in tests even for service
      // workers.
      if (web_context_->GetScriptLoaderFactory()) {
        return web_context_->GetScriptLoaderFactory()->CreateURLLoader(
            wrapped, freezable_task_runner, unfreezable_task_runner,
            std::move(keep_alive_handle), back_forward_cache_loader_helper);
      }
    }
  } else {
    DCHECK(!web_context_->GetScriptLoaderFactory());
  }

  return web_context_->GetURLLoaderFactory()->CreateURLLoader(
      wrapped, freezable_task_runner, unfreezable_task_runner,
      std::move(keep_alive_handle), back_forward_cache_loader_helper);
}

std::unique_ptr<WebCodeCacheLoader>
LoaderFactoryForWorker::CreateCodeCacheLoader() {
  return web_context_->CreateCodeCacheLoader(global_scope_->GetCodeCacheHost());
}

}  // namespace blink
