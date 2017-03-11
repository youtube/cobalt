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

#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "cobalt/loader/image/threaded_image_decoder_proxy.h"

namespace cobalt {
namespace loader {

namespace {

void StartThreadWithPriority(base::Thread* thread,
                             const base::ThreadPriority priority) {
  DCHECK(thread != NULL);
  base::Thread::Options thread_options(MessageLoop::TYPE_DEFAULT,
                                       0 /* default stack size */, priority);
  thread->StartWithOptions(thread_options);
}

}  // namespace

LoaderFactory::LoaderFactory(FetcherFactory* fetcher_factory,
    render_tree::ResourceProvider* resource_provider,
    base::ThreadPriority software_decoder_thread_priority,
    base::ThreadPriority hardware_decoder_thread_priority,
    base::ThreadPriority fetcher_thread_priority)
    : fetcher_factory_(fetcher_factory),
      resource_provider_(resource_provider),
      software_decoder_thread_(new base::Thread("SoftwareDecoderThread")),
      hardware_decoder_thread_(new base::Thread("HardwareDecoderThread")),
      fetcher_thread_(new base::Thread("FetcherThread")) {
  StartThreadWithPriority(software_decoder_thread_.get(),
                          software_decoder_thread_priority);
  StartThreadWithPriority(hardware_decoder_thread_.get(),
                          hardware_decoder_thread_priority);
  StartThreadWithPriority(fetcher_thread_.get(), fetcher_thread_priority);
}

scoped_ptr<Loader> LoaderFactory::CreateImageLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const image::ImageDecoder::SuccessCallback& success_callback,
    const image::ImageDecoder::ErrorCallback& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(fetcher_thread_);
  DCHECK(software_decoder_thread_);
  DCHECK(hardware_decoder_thread_);

  scoped_ptr<Loader> loader(new Loader(
      MakeFetcherCreator(url, url_security_callback,
                         fetcher_thread_->message_loop()),
      scoped_ptr<Decoder>(new image::ThreadedImageDecoderProxy(
          resource_provider_, success_callback, error_callback,
          software_decoder_thread_->message_loop(),
          hardware_decoder_thread_->message_loop())),
      error_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this))));
  OnLoaderCreated(loader.get());
  return loader.Pass();
}

scoped_ptr<Loader> LoaderFactory::CreateTypefaceLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const font::TypefaceDecoder::SuccessCallback& success_callback,
    const font::TypefaceDecoder::ErrorCallback& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<Loader> loader(new Loader(
      MakeFetcherCreator(url, url_security_callback, NULL),
      scoped_ptr<Decoder>(
          new font::TypefaceDecoder(resource_provider_, success_callback,
                                    error_callback)),
      error_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this))));
  OnLoaderCreated(loader.get());
  return loader.Pass();
}

// Creates a loader that fetches and decodes a Mesh.
scoped_ptr<Loader> LoaderFactory::CreateMeshLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const mesh::MeshDecoder::SuccessCallback& success_callback,
    const mesh::MeshDecoder::ErrorCallback& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<Loader> loader(new Loader(
      MakeFetcherCreator(url, url_security_callback, NULL),
      scoped_ptr<Decoder>(
          new mesh::MeshDecoder(resource_provider_, success_callback,
                                error_callback)),
      error_callback,
      base::Bind(&LoaderFactory::OnLoaderDestroyed, base::Unretained(this))));
  OnLoaderCreated(loader.get());
  return loader.Pass();
}

Loader::FetcherCreator LoaderFactory::MakeFetcherCreator(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    MessageLoop* message_loop) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return base::Bind(&FetcherFactory::CreateSecureFetcherWithMessageLoop,
                    base::Unretained(fetcher_factory_), url,
                    url_security_callback, message_loop);
}

void LoaderFactory::Suspend() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(resource_provider_);
  DCHECK(fetcher_thread_);
  DCHECK(software_decoder_thread_);
  DCHECK(hardware_decoder_thread_);

  resource_provider_ = NULL;
  for (LoaderSet::const_iterator iter = active_loaders_.begin();
       iter != active_loaders_.end(); ++iter) {
    (*iter)->Suspend();
  }

  // Wait for all fetcher thread messages to be flushed before
  // returning.
  base::WaitableEvent messages_flushed(true, false);
  if (fetcher_thread_) {
    fetcher_thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&messages_flushed)));
  messages_flushed.Wait();
  }

  // Wait for all decoder thread messages to be flushed before returning.
  if (hardware_decoder_thread_) {
    messages_flushed.Reset();
    hardware_decoder_thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&messages_flushed)));
    messages_flushed.Wait();
  }
  if (software_decoder_thread_) {
    messages_flushed.Reset();
    software_decoder_thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&messages_flushed)));
    messages_flushed.Wait();
  }
}

void LoaderFactory::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(resource_provider);
  DCHECK(!resource_provider_);

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
