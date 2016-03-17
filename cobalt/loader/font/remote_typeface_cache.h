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

#ifndef COBALT_LOADER_FONT_REMOTE_TYPEFACE_CACHE_H_
#define COBALT_LOADER_FONT_REMOTE_TYPEFACE_CACHE_H_

#include <string>

#include "cobalt/loader/font/typeface_decoder.h"
#include "cobalt/loader/resource_cache.h"

namespace cobalt {

namespace render_tree {

class Typeface;

}  // render_tree

namespace loader {
namespace font {

// |RemoteTypefaceDecoderProvider| provides a mechanism for |ResourceCache| to
// create typeface decoders, without requiring it to know anything about the
// render_tree.
struct RemoteTypefaceDecoderProvider {
 public:
  explicit RemoteTypefaceDecoderProvider(
      render_tree::ResourceProvider* resource_provider)
      : resource_provider_(resource_provider) {}

  scoped_ptr<Decoder> CreateDecoder(
      const TypefaceDecoder::SuccessCallback& success_callback,
      const TypefaceDecoder::ErrorCallback& error_callback) const {
    return make_scoped_ptr<Decoder>(new TypefaceDecoder(
        resource_provider_, success_callback, error_callback));
  }

 private:
  render_tree::ResourceProvider* resource_provider_;
};

// |RemoteTypefaceResourceCacheType| provides the types and implements the
// functions required by |ResourceCache<CacheType>|
struct RemoteTypefaceResourceCacheType {
  typedef render_tree::Typeface ResourceType;
  typedef RemoteTypefaceDecoderProvider DecoderProviderType;

  static uint32 GetEstimatedSizeInBytes(
      const scoped_refptr<ResourceType>& resource) {
    return resource->GetEstimatedSizeInBytes();
  }
};

typedef CachedResource<RemoteTypefaceResourceCacheType> CachedRemoteTypeface;
typedef CachedResourceReferenceWithCallbacks<RemoteTypefaceResourceCacheType>
    CachedRemoteTypefaceReferenceWithCallbacks;
typedef CachedRemoteTypefaceReferenceWithCallbacks::
    CachedResourceReferenceVector CachedRemoteTypefaceReferenceVector;

typedef ResourceCache<RemoteTypefaceResourceCacheType> RemoteTypefaceCache;

// CreateTypefaceCache() provides a mechanism for creating a remote typeface
// cache, without requiring the caller to deal with the typeface decoder
// provider
inline static scoped_ptr<RemoteTypefaceCache> CreateRemoteTypefaceCache(
    const std::string& name, uint32 cache_capacity,
    render_tree::ResourceProvider* resource_provider,
    loader::FetcherFactory* fetcher_factory) {
  return make_scoped_ptr<RemoteTypefaceCache>(new RemoteTypefaceCache(
      name, cache_capacity,
      make_scoped_ptr<RemoteTypefaceDecoderProvider>(
          new RemoteTypefaceDecoderProvider(resource_provider)),
      fetcher_factory));
}

}  // namespace font
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FONT_REMOTE_TYPEFACE_CACHE_H_
