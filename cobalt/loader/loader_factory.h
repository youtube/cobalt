/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_LOADER_LOADER_FACTORY_H_
#define COBALT_LOADER_LOADER_FACTORY_H_

#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/font/typeface_decoder.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/loader/loader.h"
#include "cobalt/render_tree/resource_provider.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace loader {

// The LoaderFactory provides a central loader creator object from which clients
// can request the creation of loaders of various types.  The LoaderFactory
// maintains all context necessary to create the various resource types.
class LoaderFactory {
 public:
  LoaderFactory(FetcherFactory* fetcher_factory,
                render_tree::ResourceProvider* resource_provider);

  // Creates a loader that fetches and decodes a render_tree::Image.
  scoped_ptr<Loader> CreateImageLoader(
      const GURL& url, const csp::SecurityCallback& url_security_callback,
      const image::ImageDecoder::SuccessCallback& success_callback,
      const image::ImageDecoder::FailureCallback& failure_callback,
      const image::ImageDecoder::ErrorCallback& error_callback);

  // Creates a loader that fetches and decodes a render_tree::Typeface.
  scoped_ptr<Loader> CreateTypefaceLoader(
      const GURL& url, const csp::SecurityCallback& url_security_callback,
      const font::TypefaceDecoder::SuccessCallback& success_callback,
      const font::TypefaceDecoder::FailureCallback& failure_callback,
      const font::TypefaceDecoder::ErrorCallback& error_callback);

 private:
  Loader::FetcherCreator MakeFetcherCreator(
      const GURL& url, const csp::SecurityCallback& url_security_callback);

  // Used to create the Fetcher component of the loaders.
  FetcherFactory* fetcher_factory_;

  // Used to create render_tree resources.
  render_tree::ResourceProvider* resource_provider_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_LOADER_FACTORY_H_
