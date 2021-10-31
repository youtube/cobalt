// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_URL_H_
#define COBALT_DOM_URL_H_

#include <string>

#include "cobalt/dom/blob.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/loader/blob_fetcher.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The URL interface contains static functions CreateObjectURL() and
// RevokeObjectURL().  The former is used to create urls in string from
// JavaScript blob objects.  The latter is used to revoke the url.
//   https://www.w3.org/TR/2015/WD-FileAPI-20150421/#URL-object
//
// The Media Source Extension extends it to create an url from a MediaSource
// object so we can assign it to HTMLMediaElement.src.
//   https://rawgit.com/w3c/media-source/cfb1b3d4309a6e6e2c01bd87e048758172a86e4b/media-source.html#dom-createobjecturl
class URL : public script::Wrappable {
 public:
  static std::string CreateObjectURL(
      script::EnvironmentSettings* environment_settings,
      const scoped_refptr<MediaSource>& media_source);
  static std::string CreateObjectURL(
      script::EnvironmentSettings* environment_settings,
      const scoped_refptr<Blob>& blob);
  static void RevokeObjectURL(script::EnvironmentSettings* environment_settings,
                              const std::string& url);

  static loader::BlobFetcher::ResolverCallback MakeBlobResolverCallback(
      dom::Blob::Registry* blob_registry) {
    DCHECK(blob_registry);
    return base::Bind(&BlobResolver, blob_registry);
  }

  DEFINE_WRAPPABLE_TYPE(URL);

 private:
  static bool BlobResolver(dom::Blob::Registry* registry, const GURL& url,
                           const char** data, size_t* size);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_URL_H_
