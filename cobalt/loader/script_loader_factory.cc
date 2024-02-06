// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/script_loader_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/threading/platform_thread.h"
#include "cobalt/loader/image/threaded_image_decoder_proxy.h"

namespace cobalt {
namespace loader {

namespace {

// The ResourceLoader thread uses the default stack size, which is requested
// by passing in 0 for its stack size.
const size_t kLoadThreadStackSize = 0;

}  // namespace

ScriptLoaderFactory::ScriptLoaderFactory(
    const char* name, FetcherFactory* fetcher_factory,
    base::ThreadType loader_thread_priority)
    : fetcher_factory_(fetcher_factory),
      load_thread_("ResourceLoader"),
      is_suspended_(false) {
#ifndef USE_HACKY_COBALT_CHANGES
  base::Thread::Options options(base::MessageLoop::TYPE_DEFAULT,
                                kLoadThreadStackSize);
  options.priority = loader_thread_priority;
  load_thread_.StartWithOptions(options);
#else
  load_thread_.StartWithOptions(base::Thread::Options(
      base::MessageLoop::TYPE_DEFAULT, kLoadThreadStackSize));
#endif
}

std::unique_ptr<Loader> ScriptLoaderFactory::CreateScriptLoader(
    const GURL& url, const Origin& origin,
    const csp::SecurityCallback& url_security_callback,
    const TextDecoder::TextAvailableCallback& script_available_callback,
    const TextDecoder::ResponseStartedCallback& response_started_callback,
    const Loader::OnCompleteFunction& load_complete_callback,
    net::HttpRequestHeaders headers, bool skip_fetch_intercept) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Loader::FetcherCreator fetcher_creator =
      MakeFetcherCreator(url, url_security_callback, kNoCORSMode, origin,
                         network::disk_cache::kUncompiledScript,
                         std::move(headers), skip_fetch_intercept);

  std::unique_ptr<Loader> loader(
      new Loader(fetcher_creator,
                 base::Bind(&TextDecoder::Create, script_available_callback,
                            response_started_callback),
                 load_complete_callback,
                 base::Bind(&ScriptLoaderFactory::OnLoaderDestroyed,
                            base::Unretained(this)),
                 is_suspended_));

  OnLoaderCreated(loader.get());
  return loader;
}

Loader::FetcherCreator ScriptLoaderFactory::MakeFetcherCreator(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    RequestMode request_mode, const Origin& origin,
    network::disk_cache::ResourceType type, net::HttpRequestHeaders headers,
    bool skip_fetch_intercept) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return base::Bind(
      &FetcherFactory::CreateSecureFetcher, base::Unretained(fetcher_factory_),
      url, /*main_resource=*/false, url_security_callback, request_mode, origin,
      type, std::move(headers), skip_fetch_intercept);
}

void ScriptLoaderFactory::OnLoaderCreated(Loader* loader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(active_loaders_.find(loader) == active_loaders_.end());
  active_loaders_.insert(loader);
}

void ScriptLoaderFactory::OnLoaderDestroyed(Loader* loader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(active_loaders_.find(loader) != active_loaders_.end());
  active_loaders_.erase(loader);
}

}  // namespace loader
}  // namespace cobalt
