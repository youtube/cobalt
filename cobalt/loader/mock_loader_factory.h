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

#ifndef COBALT_LOADER_MOCK_LOADER_FACTORY_H_
#define COBALT_LOADER_MOCK_LOADER_FACTORY_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/font/typeface_decoder.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/loader_factory.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace loader {

class MockLoaderFactory {
 public:
  MOCK_METHOD5(CreateImageLoaderMock,
               loader::Loader*(
                   const GURL& url, const Origin&,
                   const csp::SecurityCallback& url_security_callback,
                   const image::ImageDecoder::SuccessCallback& success_callback,
                   const image::ImageDecoder::ErrorCallback& error_callback));

  MOCK_METHOD5(
      CreateTypefaceLoaderMock,
      loader::Loader*(
          const GURL& url, const Origin&,
          const csp::SecurityCallback& url_security_callback,
          const font::TypefaceDecoder::SuccessCallback& success_callback,
          const font::TypefaceDecoder::ErrorCallback& error_callback));

  // This workaround is required since scoped_ptr has no copy constructor.
  // see:
  // https://groups.google.com/a/chromium.org/forum/#!msg/chromium-dev/01sDxsJ1OYw/I_S0xCBRF2oJ
  scoped_ptr<Loader> CreateImageLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const image::ImageDecoder::SuccessCallback& success_callback,
      const image::ImageDecoder::ErrorCallback& error_callback) {
    return scoped_ptr<Loader>(CreateImageLoaderMock(
        url, origin, url_security_callback, success_callback, error_callback));
  }

  scoped_ptr<Loader> CreateTypefaceLoader(
      const GURL& url, const Origin& origin,
      const csp::SecurityCallback& url_security_callback,
      const font::TypefaceDecoder::SuccessCallback& success_callback,
      const font::TypefaceDecoder::ErrorCallback& error_callback) {
    return scoped_ptr<Loader>(CreateTypefaceLoaderMock(
        url, origin, url_security_callback, success_callback, error_callback));
  }

  MOCK_METHOD0(Suspend, void());
  MOCK_METHOD1(Resume, void(render_tree::ResourceProvider* resource_provider));
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MOCK_LOADER_FACTORY_H_
