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

#include "cobalt/loader/loader_factory.h"

namespace cobalt {
namespace loader {

LoaderFactory::LoaderFactory(FetcherFactory* fetcher_factory,
                             render_tree::ResourceProvider* resource_provider)
    : fetcher_factory_(fetcher_factory),
      resource_provider_(resource_provider) {}

scoped_ptr<Loader> LoaderFactory::CreateImageLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const image::ImageDecoder::SuccessCallback& success_callback,
    const image::ImageDecoder::FailureCallback& failure_callback,
    const image::ImageDecoder::ErrorCallback& error_callback) {
  return make_scoped_ptr(
      new Loader(MakeFetcherCreator(url, url_security_callback),
                 scoped_ptr<Decoder>(new image::ImageDecoder(
                     resource_provider_, success_callback, failure_callback,
                     error_callback)),
                 error_callback));
}

scoped_ptr<Loader> LoaderFactory::CreateTypefaceLoader(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    const font::TypefaceDecoder::SuccessCallback& success_callback,
    const font::TypefaceDecoder::FailureCallback& failure_callback,
    const font::TypefaceDecoder::ErrorCallback& error_callback) {
  return make_scoped_ptr(
      new Loader(MakeFetcherCreator(url, url_security_callback),
                 scoped_ptr<Decoder>(new font::TypefaceDecoder(
                     resource_provider_, success_callback, failure_callback,
                     error_callback)),
                 error_callback));
}

Loader::FetcherCreator LoaderFactory::MakeFetcherCreator(
    const GURL& url, const csp::SecurityCallback& url_security_callback) {
  return base::Bind(&FetcherFactory::CreateSecureFetcher,
                    base::Unretained(fetcher_factory_), url,
                    url_security_callback);
}

}  // namespace loader
}  // namespace cobalt
