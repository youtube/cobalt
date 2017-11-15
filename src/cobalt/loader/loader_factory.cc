// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "base/threading/platform_thread.h"
#include "cobalt/loader/image/threaded_image_decoder_proxy.h"

namespace cobalt {
namespace loader {

namespace {

// The ResourceLoader thread uses the default stack size, which is requested
// by passing in 0 for its stack size.
const size_t kLoadThreadStackSize = 0;

}  // namespace

LoaderFactory::LoaderFactory(FetcherFactory* fetcher_factory,
                             render_tree::ResourceProvider* resource_provider,
                             base::ThreadPriority loader_thread_priority)
    : fetcher_factory_(fetcher_factory),
      resource_provider_(resource_provider),
      load_thread_("ResourceLoader"),
      is_suspended_(false) {
  base::Thread::Options options(MessageLoop::TYPE_DEFAULT, kLoadThreadStackSize,
                                loader_thread_priority);
  load_thread_.StartWithOptions(options);
}

scoped_ptr<Loader> LoaderFactory::CreateImageLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const image::ImageDecoder::SuccessCallback& success_callback,
    const image::ImageDecoder::ErrorCallback& error_callback,
    const Origin& origin) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<Loader> loader(new Loader(
      MakeFetcherCreator(url, url_security_callback, kNoCORSMode, origin),
      scoped_ptr<Decoder>(new image::ThreadedImageDecoderProxy(
          resource_provider_, success_callback, error_callback,
          load_thread_.message_loop())),
      error_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));
  OnLoaderCreated(loader.get());
  return loader.Pass();
}

scoped_ptr<Loader> LoaderFactory::CreateTypefaceLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const font::TypefaceDecoder::SuccessCallback& success_callback,
    const font::TypefaceDecoder::ErrorCallback& error_callback,
    const Origin& origin) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<Loader> loader(new Loader(
      MakeFetcherCreator(url, url_security_callback,
                         kCORSModeSameOriginCredentials, origin),
      scoped_ptr<Decoder>(new font::TypefaceDecoder(
          resource_provider_, success_callback, error_callback)),
      error_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));
  OnLoaderCreated(loader.get());
  return loader.Pass();
}

// Creates a loader that fetches and decodes a Mesh.
scoped_ptr<Loader> LoaderFactory::CreateMeshLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const mesh::MeshDecoder::SuccessCallback& success_callback,
    const mesh::MeshDecoder::ErrorCallback& error_callback,
    const Origin& origin) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<Loader> loader(new Loader(
      MakeFetcherCreator(url, url_security_callback, kNoCORSMode, origin),
      scoped_ptr<Decoder>(new mesh::MeshDecoder(
          resource_provider_, success_callback, error_callback)),
      error_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this)),
      is_suspended_));
  OnLoaderCreated(loader.get());
  return loader.Pass();
}

Loader::FetcherCreator LoaderFactory::MakeFetcherCreator(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    RequestMode request_mode, const Origin& origin) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return base::Bind(&FetcherFactory::CreateSecureFetcher,
                    base::Unretained(fetcher_factory_), url,
                    url_security_callback, request_mode, origin);
}

void LoaderFactory::Suspend() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(resource_provider_);
  DCHECK(!is_suspended_);

  is_suspended_ = true;
  resource_provider_ = NULL;

  for (LoaderSet::const_iterator iter = active_loaders_.begin();
       iter != active_loaders_.end(); ++iter) {
    (*iter)->Suspend();
  }

  // Wait for all loader thread messages to be flushed before returning.
  load_thread_.message_loop()->WaitForFence();
}

void LoaderFactory::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(resource_provider);

  is_suspended_ = false;
  resource_provider_ = resource_provider;

  for (LoaderSet::const_iterator iter = active_loaders_.begin();
       iter != active_loaders_.end(); ++iter) {
    (*iter)->Resume(resource_provider);
  }
}

void LoaderFactory::OnLoaderCreated(Loader* loader) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(active_loaders_.find(loader) == active_loaders_.end());
  active_loaders_.insert(loader);
}

void LoaderFactory::OnLoaderDestroyed(Loader* loader) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(active_loaders_.find(loader) != active_loaders_.end());
  active_loaders_.erase(loader);
}

}  // namespace loader
}  // namespace cobalt
