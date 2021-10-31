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

#ifndef COBALT_LOADER_LOADER_FACTORY_H_
#define COBALT_LOADER_LOADER_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/threading/thread.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/fetcher.h"
#include "cobalt/loader/fetcher_cache.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/font/typeface_decoder.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/mesh/mesh_decoder.h"
#include "cobalt/loader/text_decoder.h"
#include "cobalt/render_tree/resource_provider.h"
#include "url/gurl.h"

namespace cobalt {
namespace loader {

// The LoaderFactory provides a central loader creator object from which clients
// can request the creation of loaders of various types.  The LoaderFactory
// maintains all context necessary to create the various resource types.
class LoaderFactory {
 public:
  LoaderFactory(const char* name, FetcherFactory* fetcher_factory,
                render_tree::ResourceProvider* resource_provider,
                const base::DebuggerHooks& debugger_hooks,
                size_t encoded_image_cache_capacity,
                base::ThreadPriority loader_thread_priority);

  // Creates a loader that fetches and decodes an image.
  std::unique_ptr<Loader> CreateImageLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const image::ImageDecoder::ImageAvailableCallback&
          image_available_callback,
      const Loader::OnCompleteFunction& load_complete_callback);

  // Creates a loader that fetches and decodes a link resources.
  std::unique_ptr<Loader> CreateLinkLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const loader::RequestMode cors_mode,
      const TextDecoder::TextAvailableCallback& link_available_callback,
      const Loader::OnCompleteFunction& load_complete_callback);

  // Creates a loader that fetches and decodes a Mesh.
  std::unique_ptr<Loader> CreateMeshLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const mesh::MeshDecoder::MeshAvailableCallback& mesh_available_callback,
      const Loader::OnCompleteFunction& load_complete_callback);

  // Creates a loader that fetches and decodes a Javascript resource.
  std::unique_ptr<Loader> CreateScriptLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const TextDecoder::TextAvailableCallback& script_available_callback,
      const Loader::OnCompleteFunction& load_complete_callback);

  // Creates a loader that fetches and decodes a render_tree::Typeface.
  std::unique_ptr<Loader> CreateTypefaceLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const font::TypefaceDecoder::TypefaceAvailableCallback&
          typeface_available_callback,
      const Loader::OnCompleteFunction& load_complete_callback);

  // Notify the LoaderFactory that the resource identified by "url" is being
  // requested again.
  void NotifyResourceRequested(const std::string& url);

  // Clears out the loader factory's resource provider, aborting any in-progress
  // loads.
  void Suspend();
  // Resets a new resource provider for this loader factory to use.  The
  // previous resource provider must have been cleared before this method is
  // called.
  void Resume(render_tree::ResourceProvider* resource_provider);

  // Resets a new resource provider for this loader factory to use.  The
  // previous resource provider must have been cleared before this method is
  // called.
  void UpdateResourceProvider(render_tree::ResourceProvider* resource_provider);

 private:
  void OnLoaderCreated(Loader* loader);
  void OnLoaderDestroyed(Loader* loader);
  void SuspendActiveLoaders();
  void ResumeActiveLoaders(render_tree::ResourceProvider* resource_provider);

  Loader::FetcherCreator MakeFetcherCreator(
      const GURL& url, const csp::SecurityCallback& url_security_callback,
      RequestMode request_mode, const Origin& origin);
  Loader::FetcherCreator MakeCachedFetcherCreator(
      const GURL& url, const csp::SecurityCallback& url_security_callback,
      RequestMode request_mode, const Origin& origin);

  // Ensures that the LoaderFactory methods are only called from the same
  // thread.
  THREAD_CHECKER(thread_checker_);

  // Used to create the Fetcher component of the loaders.
  FetcherFactory* fetcher_factory_;

  // Used to cache the fetched raw data.  Note that currently the cache is only
  // used to cache Image data.  We may introduce more caches once we want to
  // cache fetched data for other resource types.
  std::unique_ptr<FetcherCache> fetcher_cache_;

  // Used to create render_tree resources.
  render_tree::ResourceProvider* resource_provider_;

  // Used with CLOG to report errors with the image source.
  const base::DebuggerHooks& debugger_hooks_;

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

#endif  // COBALT_LOADER_LOADER_FACTORY_H_
