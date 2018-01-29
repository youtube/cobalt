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

#ifndef COBALT_LOADER_FONT_REMOTE_TYPEFACE_CACHE_H_
#define COBALT_LOADER_FONT_REMOTE_TYPEFACE_CACHE_H_

#include <string>

#include "cobalt/loader/loader_factory.h"
#include "cobalt/loader/resource_cache.h"
#include "cobalt/render_tree/typeface.h"

namespace cobalt {

namespace render_tree {

class Typeface;

}  // render_tree

namespace loader {
namespace font {

// |RemoteTypefaceResourceCacheType| provides the types and implements the
// functions required by |ResourceCache<CacheType>|
struct RemoteTypefaceResourceCacheType {
  typedef render_tree::Typeface ResourceType;

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
// cache.
inline static scoped_ptr<RemoteTypefaceCache> CreateRemoteTypefaceCache(
    const std::string& name, uint32 cache_capacity,
    loader::LoaderFactory* loader_factory) {
  return make_scoped_ptr<RemoteTypefaceCache>(new RemoteTypefaceCache(
      name, cache_capacity, true /*are_loading_retries_enabled*/,
      base::Bind(&loader::LoaderFactory::CreateTypefaceLoader,
                 base::Unretained(loader_factory))));
}

}  // namespace font
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FONT_REMOTE_TYPEFACE_CACHE_H_
