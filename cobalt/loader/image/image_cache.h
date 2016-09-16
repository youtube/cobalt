/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_IMAGE_CACHE_H_
#define COBALT_LOADER_IMAGE_IMAGE_CACHE_H_

#include <string>

#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/loader/resource_cache.h"

namespace cobalt {
namespace loader {
namespace image {

// |ImageDecoderProvider| provides a mechanism for |ResourceCache| to create
// decoders, without requiring it to know anything about the render_tree.
struct ImageDecoderProvider {
 public:
  explicit ImageDecoderProvider(
      render_tree::ResourceProvider* resource_provider)
      : resource_provider_(resource_provider) {}

  scoped_ptr<Decoder> CreateDecoder(
      const ImageDecoder::SuccessCallback& success_callback,
      const ImageDecoder::FailureCallback& failure_callback,
      const ImageDecoder::ErrorCallback& error_callback) const {
    return make_scoped_ptr<Decoder>(
        new ImageDecoder(resource_provider_, success_callback, failure_callback,
                         error_callback));
  }

 private:
  render_tree::ResourceProvider* resource_provider_;
};

// |ImageResourceCacheType| provides the types and implements the functions
// required by |ResourceCache<ImageResourceCacheType>|
struct ImageResourceCacheType {
  typedef render_tree::Image ResourceType;
  typedef ImageDecoderProvider DecoderProviderType;

  static uint32 GetEstimatedSizeInBytes(
      const scoped_refptr<ResourceType>& resource) {
    return resource->GetEstimatedSizeInBytes();
  }
};

typedef CachedResource<ImageResourceCacheType> CachedImage;
typedef CachedResourceReferenceWithCallbacks<ImageResourceCacheType>
    CachedImageReferenceWithCallbacks;
typedef CachedImageReferenceWithCallbacks::CachedResourceReferenceVector
    CachedImageReferenceVector;

typedef ResourceCache<ImageResourceCacheType> ImageCache;

// CreateImageCache() provides a mechanism for creating an |ImageCache|, without
// requiring the caller to deal with the |ImageDecoderProvider|
inline static scoped_ptr<ImageCache> CreateImageCache(
    const std::string& name, uint32 cache_capacity,
    render_tree::ResourceProvider* resource_provider,
    loader::FetcherFactory* fetcher_factory) {
  return make_scoped_ptr<ImageCache>(new ImageCache(
      name, cache_capacity, make_scoped_ptr<ImageDecoderProvider>(
                                new ImageDecoderProvider(resource_provider)),
      fetcher_factory));
}

// The ReducedCacheCapacityManager is a helper class that manages state which
// makes it easy for clients to place the image cache in a reduced memory state,
// at times when GPU memory is at a premium, such as when playing a video.
// Clients should create ReducedCacheCapacityManager::Request objects to
// indicate that they would like the image cache to enter a reduced capacity
// state, internally, the manager keeps a reference count of how many Request
// objects exist and enables the reduced capacity state if there is more than
// one of them.
class ReducedCacheCapacityManager {
 public:
  class Request {
   public:
    explicit Request(ReducedCacheCapacityManager* manager) : manager_(manager) {
      manager_->IncrementRequestRefCount();
    }
    ~Request() { manager_->DecrementRequestRefCount(); }

   private:
    ReducedCacheCapacityManager* manager_;
  };

  ReducedCacheCapacityManager(ImageCache* cache,
                              float reduced_capacity_percentage)
      : cache_(cache),
        request_ref_count_(0),
        reduced_capacity_percentage_(reduced_capacity_percentage),
        original_capacity_(cache_->capacity()),
        reduced_capacity_(static_cast<uint32>(reduced_capacity_percentage_ *
                                              original_capacity_)) {
    DCHECK_GE(1.0f, reduced_capacity_percentage);
  }

  float reduced_capacity_percentage() const {
    return reduced_capacity_percentage_;
  }

 private:
  void IncrementRequestRefCount() {
    if (request_ref_count_ == 0) {
      cache_->SetCapacity(reduced_capacity_);
    }
    ++request_ref_count_;
  }

  void DecrementRequestRefCount() {
    DCHECK_LT(0, request_ref_count_);
    --request_ref_count_;
    if (request_ref_count_ == 0) {
      cache_->SetCapacity(original_capacity_);
    }
  }

  ImageCache* cache_;
  int request_ref_count_;
  float reduced_capacity_percentage_;
  const uint32 original_capacity_;
  const uint32 reduced_capacity_;

  friend class Request;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_CACHE_H_
