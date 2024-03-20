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

#ifndef COBALT_LOADER_SCRIPT_LOADER_FACTORY_H_
#define COBALT_LOADER_SCRIPT_LOADER_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/threading/thread.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/fetcher.h"
#include "cobalt/loader/fetcher_cache.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/text_decoder.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace cobalt {
namespace loader {

// The LoaderFactory provides a central loader creator object from which clients
// can request the creation of loaders of various types.  The LoaderFactory
// maintains all context necessary to create the various resource types.
class ScriptLoaderFactory {
 public:
  ScriptLoaderFactory(
      const char* name, FetcherFactory* fetcher_factory,
      base::ThreadType loader_thread_priority = base::ThreadType::kDefault);

  // Creates a loader that fetches and decodes a Javascript resource.
  std::unique_ptr<Loader> CreateScriptLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const TextDecoder::TextAvailableCallback& script_available_callback,
      const Loader::OnCompleteFunction& load_complete_callback,
      bool skip_fetch_intercept = false) {
    return CreateScriptLoader(
        url, origin, url_security_callback, script_available_callback,
        TextDecoder::ResponseStartedCallback(), load_complete_callback,
        net::HttpRequestHeaders(), skip_fetch_intercept);
  }

  std::unique_ptr<Loader> CreateScriptLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const TextDecoder::TextAvailableCallback& script_available_callback,
      const TextDecoder::ResponseStartedCallback& response_started_callback,
      const Loader::OnCompleteFunction& load_complete_callback,
      net::HttpRequestHeaders headers = net::HttpRequestHeaders(),
      bool skip_fetch_intercept = false);

 protected:
  void OnLoaderCreated(Loader* loader);
  void OnLoaderDestroyed(Loader* loader);

  Loader::FetcherCreator MakeFetcherCreator(
      const GURL& url, const csp::SecurityCallback& url_security_callback,
      RequestMode request_mode, const Origin& origin,
      network::disk_cache::ResourceType type = network::disk_cache::kOther,
      net::HttpRequestHeaders headers = net::HttpRequestHeaders(),
      bool skip_fetch_intercept = false);

  // Ensures that the LoaderFactory methods are only called from the same
  // thread.
  THREAD_CHECKER(thread_checker_);

  // Used to create the Fetcher component of the loaders.
  FetcherFactory* fetcher_factory_;

  // Keeps track of all active loaders so that if a suspend event occurs they
  // can be aborted.
  typedef std::set<Loader*> LoaderSet;
  LoaderSet active_loaders_;

  // Thread to run asynchronous fetchers and decoders on.  At the moment,
  // image decoding is the only thing done on this thread.
  base::Thread load_thread_;

  // Whether or not the LoaderFactory is currently suspended. While it is, all
  // loaders created by it begin in a suspended state.
  bool is_suspended_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_SCRIPT_LOADER_FACTORY_H_
