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

#include "cobalt/loader/loader_factory.h"

#include <memory>

#include "base/threading/platform_thread.h"
#include "cobalt/loader/image/threaded_image_decoder_proxy.h"

namespace cobalt {
namespace loader {

LoaderFactory::LoaderFactory(const char* name, FetcherFactory* fetcher_factory,
                             render_tree::ResourceProvider* resource_provider,
                             const base::DebuggerHooks& debugger_hooks,
                             size_t encoded_image_cache_capacity,
                             base::ThreadPriority loader_thread_priority)
    : ScriptLoaderFactory(name, fetcher_factory, loader_thread_priority),
      debugger_hooks_(debugger_hooks),
      resource_provider_(resource_provider) {
  if (encoded_image_cache_capacity > 0) {
    fetcher_cache_ = new FetcherCache(name, encoded_image_cache_capacity);
  }
}

LoaderFactory::~LoaderFactory() {
  if (fetcher_cache_) {
    fetcher_cache_->DestroySoon();
  }
}

std::unique_ptr<Loader> LoaderFactory::CreateImageLoader(
    const GURL& url, const Origin& origin,
    const csp::SecurityCallback& url_security_callback,
    const image::ImageDecoder::ImageAvailableCallback& image_available_callback,
    const Loader::OnCompleteFunction& load_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Loader::FetcherCreator fetcher_creator = MakeCachedFetcherCreator(
      url, url_security_callback, kNoCORSMode, origin, disk_cache::kImage);

  std::unique_ptr<Loader> loader(new Loader(
      fetcher_creator,
      base::Bind(&image::ThreadedImageDecoderProxy::Create, resource_provider_,
                 &debugger_hooks_, image_available_callback,
                 load_thread_.message_loop()),
      load_complete_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));

  OnLoaderCreated(loader.get());
  return loader;
}

std::unique_ptr<Loader> LoaderFactory::CreateLinkLoader(
    const GURL& url, const Origin& origin,
    const csp::SecurityCallback& url_security_callback,
    const loader::RequestMode cors_mode, const disk_cache::ResourceType type,
    const TextDecoder::TextAvailableCallback& link_available_callback,
    const Loader::OnCompleteFunction& load_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Loader::FetcherCreator fetcher_creator =
      MakeFetcherCreator(url, url_security_callback, cors_mode, origin, type);

  std::unique_ptr<Loader> loader(new Loader(
      fetcher_creator,
      base::Bind(&loader::TextDecoder::Create, link_available_callback,
                 loader::TextDecoder::ResponseStartedCallback()),
      load_complete_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));

  OnLoaderCreated(loader.get());
  return loader;
}

// Creates a loader that fetches and decodes a Mesh.
std::unique_ptr<Loader> LoaderFactory::CreateMeshLoader(
    const GURL& url, const Origin& origin,
    const csp::SecurityCallback& url_security_callback,
    const mesh::MeshDecoder::MeshAvailableCallback& mesh_available_callback,
    const Loader::OnCompleteFunction& load_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Loader::FetcherCreator fetcher_creator =
      MakeFetcherCreator(url, url_security_callback, kNoCORSMode, origin);

  std::unique_ptr<Loader> loader(new Loader(
      fetcher_creator,
      base::Bind(&mesh::MeshDecoder::Create, resource_provider_,
                 mesh_available_callback),
      load_complete_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));

  OnLoaderCreated(loader.get());
  return loader;
}

std::unique_ptr<Loader> LoaderFactory::CreateTypefaceLoader(
    const GURL& url, const Origin& origin,
    const csp::SecurityCallback& url_security_callback,
    const font::TypefaceDecoder::TypefaceAvailableCallback&
        typeface_available_callback,
    const Loader::OnCompleteFunction& load_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Loader::FetcherCreator fetcher_creator = MakeFetcherCreator(
      url, url_security_callback, kCORSModeSameOriginCredentials, origin,
      disk_cache::kFont);

  std::unique_ptr<Loader> loader(new Loader(
      fetcher_creator,
      base::Bind(&font::TypefaceDecoder::Create, resource_provider_,
                 typeface_available_callback),
      load_complete_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));

  OnLoaderCreated(loader.get());
  return loader;
}

Loader::FetcherCreator LoaderFactory::MakeCachedFetcherCreator(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    RequestMode request_mode, const Origin& origin,
    const disk_cache::ResourceType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto fetcher_creator = MakeFetcherCreator(url, url_security_callback,
                                            request_mode, origin, type);

  if (fetcher_cache_) {
    return fetcher_cache_->GetFetcherCreator(url, fetcher_creator);
  }
  return fetcher_creator;
}

void LoaderFactory::NotifyResourceRequested(const std::string& url) {
  if (fetcher_cache_) {
    fetcher_cache_->NotifyResourceRequested(url);
  }
}

void LoaderFactory::Suspend() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resource_provider_);
  DCHECK(!is_suspended_);

  is_suspended_ = true;
  resource_provider_ = NULL;
  SuspendActiveLoaders();
}

void LoaderFactory::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resource_provider);
  DCHECK(is_suspended_);

  is_suspended_ = false;
  ResumeActiveLoaders(resource_provider);
}

void LoaderFactory::UpdateResourceProvider(
    render_tree::ResourceProvider* resource_provider) {
  DCHECK(resource_provider);
  SuspendActiveLoaders();
  ResumeActiveLoaders(resource_provider);
}

void LoaderFactory::SuspendActiveLoaders() {
  for (LoaderSet::const_iterator iter = active_loaders_.begin();
       iter != active_loaders_.end(); ++iter) {
    (*iter)->Suspend();
  }

  // Wait for all loader thread messages to be flushed before returning.
  load_thread_.message_loop()->task_runner()->WaitForFence();
}

void LoaderFactory::ResumeActiveLoaders(
    render_tree::ResourceProvider* resource_provider) {
  resource_provider_ = resource_provider;

  for (LoaderSet::const_iterator iter = active_loaders_.begin();
       iter != active_loaders_.end(); ++iter) {
    (*iter)->Resume(resource_provider);
  }

  // Wait for all loader thread messages to be flushed before returning.
  load_thread_.message_loop()->task_runner()->WaitForFence();
}

}  // namespace loader
}  // namespace cobalt
