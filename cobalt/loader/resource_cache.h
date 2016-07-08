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

#ifndef COBALT_LOADER_RESOURCE_CACHE_H_
#define COBALT_LOADER_RESOURCE_CACHE_H_

#include <list>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/containers/linked_hash_map.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/c_val.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace loader {

// CacheType must provide the following:
//   typedef SpecificResourceType ResourceType;
//   typedef SpecificDecoderProviderType DecoderProviderType;
//   static uint32 GetEstimatedSizeInBytes(
//       const scoped_refptr<ResourceType>& resource);

template <typename CacheType>
class ResourceCache;

//////////////////////////////////////////////////////////////////////////
// CachedResource - Declarations
//////////////////////////////////////////////////////////////////////////

// CachedResource requests fetching and decoding a single resource and the
// decoded resource is stored in |resource_|. CachedResource is created by
// calling |CreateCachedResource| of the ResourceCache.
template <typename CacheType>
class CachedResource
    : public base::RefCountedThreadSafe<CachedResource<CacheType> > {
 public:
  typedef ResourceCache<CacheType> ResourceCacheType;
  typedef typename CacheType::ResourceType ResourceType;
  typedef typename CacheType::DecoderProviderType DecoderProviderType;

  // This class can be used to attach success, failure, or error callbacks to
  // CachedResource objects that are executed when the resource finishes
  // loading.
  // The callbacks are removed when the object is destroyed. If the resource has
  // already been loaded, execute the callback immediately.
  class OnLoadedCallbackHandler {
   public:
    OnLoadedCallbackHandler(
        const scoped_refptr<CachedResource>& cached_resource,
        const base::Closure& success_callback,
        const base::Closure& failure_callback,
        const base::Closure& error_callback);
    ~OnLoadedCallbackHandler();

   private:
    typedef std::list<base::Closure>::iterator CallbackListIterator;

    scoped_refptr<CachedResource> cached_resource_;
    base::Closure success_callback_;
    base::Closure failure_callback_;
    base::Closure error_callback_;

    CallbackListIterator success_callback_list_iterator_;
    CallbackListIterator failure_callback_list_iterator_;
    CallbackListIterator error_callback_list_iterator_;

    DISALLOW_COPY_AND_ASSIGN(OnLoadedCallbackHandler);
  };

  // Request fetching and decoding a single resource based on the url.
  CachedResource(const GURL& url,
                 const csp::SecurityCallback& security_callback,
                 const DecoderProviderType* decoder_provider,
                 loader::FetcherFactory* fetcher_factory,
                 ResourceCacheType* resource_cache);

  // Resource is available. CachedResource is a wrapper of the resource
  // and there is no need to fetch or load this resource again. |loader_|
  // is NULL in this case.
  CachedResource(const GURL& url, ResourceType* resource,
                 ResourceCacheType* resource_cache);

  // If the resource is available in the cache, simply returns the resource. If
  // the resource loader is in loading status or encounters an error, still
  // returns |resource_| even if it is NULL to indicate no resource is
  // available.
  scoped_refptr<ResourceType> TryGetResource();

  bool IsLoading();

  const GURL& url() const { return url_; }

 private:
  friend class base::RefCountedThreadSafe<CachedResource>;
  friend class OnLoadedCallbackHandler;

  typedef std::list<base::Closure> CallbackList;
  typedef std::list<base::Closure>::iterator CallbackListIterator;

  enum CallbackType {
    kOnLoadingSuccessCallbackType,
    kOnLoadingFailureCallbackType,
    kOnLoadingErrorCallbackType,
    kCallbackTypeCount,
  };

  ~CachedResource();

  // Callbacks for decoders.
  //
  // Notify that the resource is loaded successfully.
  void OnLoadingSuccess(const scoped_refptr<ResourceType>& resource);
  // Notify the loading failure and could be treated differently than error.
  void OnLoadingFailure(const std::string& warning);
  // Notify the loading error.
  void OnLoadingError(const std::string& error);

  // Called by |CachedResourceLoadedCallbackHandler|.
  CallbackListIterator AddCallback(CallbackType type,
                                   const base::Closure& callback);
  void RemoveCallback(CallbackType type, CallbackListIterator iterator);

  void RunCallbacks(CallbackType type);

  const GURL url_;
  scoped_refptr<ResourceType> resource_;
  ResourceCacheType* const resource_cache_;
  scoped_ptr<Loader> loader_;

  CallbackList callback_lists[kCallbackTypeCount];

  base::ThreadChecker cached_resource_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CachedResource);
};

//////////////////////////////////////////////////////////////////////////
// CachedResource::OnLoadedCallbackHandler - Definitions
//////////////////////////////////////////////////////////////////////////

template <typename CacheType>
CachedResource<CacheType>::OnLoadedCallbackHandler::OnLoadedCallbackHandler(
    const scoped_refptr<CachedResource>& cached_resource,
    const base::Closure& success_callback,
    const base::Closure& failure_callback, const base::Closure& error_callback)
    : cached_resource_(cached_resource),
      success_callback_(success_callback),
      failure_callback_(failure_callback),
      error_callback_(error_callback) {
  DCHECK(cached_resource_);

  if (!success_callback_.is_null()) {
    success_callback_list_iterator_ = cached_resource_->AddCallback(
        kOnLoadingSuccessCallbackType, success_callback_);
    if (cached_resource_->TryGetResource()) {
      success_callback_.Run();
    }
  }

  if (!failure_callback_.is_null()) {
    failure_callback_list_iterator_ = cached_resource_->AddCallback(
        kOnLoadingFailureCallbackType, failure_callback_);
  }

  if (!error_callback_.is_null()) {
    error_callback_list_iterator_ = cached_resource_->AddCallback(
        kOnLoadingErrorCallbackType, error_callback_);
  }
}

template <typename CacheType>
CachedResource<CacheType>::OnLoadedCallbackHandler::~OnLoadedCallbackHandler() {
  if (!success_callback_.is_null()) {
    cached_resource_->RemoveCallback(kOnLoadingSuccessCallbackType,
                                     success_callback_list_iterator_);
  }

  if (!failure_callback_.is_null()) {
    cached_resource_->RemoveCallback(kOnLoadingFailureCallbackType,
                                     failure_callback_list_iterator_);
  }

  if (!error_callback_.is_null()) {
    cached_resource_->RemoveCallback(kOnLoadingErrorCallbackType,
                                     error_callback_list_iterator_);
  }
}

//////////////////////////////////////////////////////////////////////////
// CachedResource - Definitions
//////////////////////////////////////////////////////////////////////////

template <typename CacheType>
CachedResource<CacheType>::CachedResource(
    const GURL& url, const csp::SecurityCallback& security_callback,
    const DecoderProviderType* decoder_provider,
    loader::FetcherFactory* fetcher_factory, ResourceCacheType* resource_cache)
    : url_(url), resource_cache_(resource_cache) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  scoped_ptr<Decoder> decoder = decoder_provider->CreateDecoder(
      base::Bind(&CachedResource::OnLoadingSuccess, base::Unretained(this)),
      base::Bind(&CachedResource::OnLoadingFailure, base::Unretained(this)),
      base::Bind(&CachedResource::OnLoadingError, base::Unretained(this)));
  loader_ = make_scoped_ptr(new Loader(
      base::Bind(&FetcherFactory::CreateSecureFetcher,
                 base::Unretained(fetcher_factory), url, security_callback),
      decoder.Pass(),
      base::Bind(&CachedResource::OnLoadingError, base::Unretained(this))));
}

template <typename CacheType>
CachedResource<CacheType>::CachedResource(const GURL& url,
                                          ResourceType* resource,
                                          ResourceCacheType* resource_cache)
    : url_(url), resource_(resource), resource_cache_(resource_cache) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());
}

template <typename CacheType>
scoped_refptr<typename CacheType::ResourceType>
CachedResource<CacheType>::TryGetResource() {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  return resource_;
}

template <typename CacheType>
bool CachedResource<CacheType>::IsLoading() {
  return loader_;
}

template <typename CacheType>
CachedResource<CacheType>::~CachedResource() {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  resource_cache_->NotifyResourceDestroyed(this);

  for (int i = 0; i < kCallbackTypeCount; ++i) {
    DCHECK(callback_lists[i].empty());
  }
}

template <typename CacheType>
void CachedResource<CacheType>::OnLoadingSuccess(
    const scoped_refptr<ResourceType>& resource) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  resource_ = resource;

  loader_.reset();
  resource_cache_->NotifyResourceSuccessfullyLoaded(this);
  // To avoid the last reference of this object get deleted in the callbacks.
  scoped_refptr<CachedResource<CacheType> > holder(this);
  RunCallbacks(kOnLoadingSuccessCallbackType);
}

template <typename CacheType>
void CachedResource<CacheType>::OnLoadingFailure(const std::string& message) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  LOG(WARNING) << "Warning while loading '" << url_ << "': " << message;

  loader_.reset();
  // To avoid the last reference of this object get deleted in the callbacks.
  scoped_refptr<CachedResource<CacheType> > holder(this);
  RunCallbacks(kOnLoadingFailureCallbackType);
}

template <typename CacheType>
void CachedResource<CacheType>::OnLoadingError(const std::string& error) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  LOG(ERROR) << "Error while loading '" << url_ << "': " << error;

  loader_.reset();
  // To avoid the last reference of this object get deleted in the callbacks.
  scoped_refptr<CachedResource<CacheType> > holder(this);
  RunCallbacks(kOnLoadingErrorCallbackType);
}

template <typename CacheType>
typename CachedResource<CacheType>::CallbackListIterator
CachedResource<CacheType>::AddCallback(CallbackType callback_type,
                                       const base::Closure& callback) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  CallbackList& callback_list = callback_lists[callback_type];
  callback_list.push_front(callback);
  return callback_list.begin();
}

template <typename CacheType>
void CachedResource<CacheType>::RemoveCallback(CallbackType type,
                                               CallbackListIterator iterator) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  CallbackList& callback_list = callback_lists[type];
  callback_list.erase(iterator);
}

template <typename CacheType>
void CachedResource<CacheType>::RunCallbacks(CallbackType type) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  // To avoid the list gets altered in the callbacks.
  CallbackList callback_list = callback_lists[type];
  CallbackListIterator callback_iter;
  for (callback_iter = callback_list.begin();
       callback_iter != callback_list.end(); ++callback_iter) {
    callback_iter->Run();
  }
}

//////////////////////////////////////////////////////////////////////////
// CachedResourceReferenceWithCallbacks
//////////////////////////////////////////////////////////////////////////

template <typename CacheType>
class CachedResourceReferenceWithCallbacks {
 public:
  typedef CachedResource<CacheType> CachedResourceType;
  typedef typename CachedResourceType::OnLoadedCallbackHandler
      CachedResourceTypeOnLoadedCallbackHandler;

  typedef ScopedVector<CachedResourceReferenceWithCallbacks>
      CachedResourceReferenceVector;

  CachedResourceReferenceWithCallbacks(
      const scoped_refptr<CachedResourceType>& cached_resource,
      const base::Closure& success_callback,
      const base::Closure& failure_callback,
      const base::Closure& error_callback)
      : cached_resource_(cached_resource),
        cached_resource_loaded_callback_handler_(
            new CachedResourceTypeOnLoadedCallbackHandler(
                cached_resource, success_callback, failure_callback,
                error_callback)) {}

  scoped_refptr<CachedResourceType> cached_resource() {
    return cached_resource_;
  }

 private:
  // A single cached resource.
  scoped_refptr<CachedResourceType> cached_resource_;
  // This handles adding and removing the resource loaded callbacks.
  scoped_ptr<CachedResourceTypeOnLoadedCallbackHandler>
      cached_resource_loaded_callback_handler_;
};

//////////////////////////////////////////////////////////////////////////
// ResourceCache - Declarations
//////////////////////////////////////////////////////////////////////////

// CachedResource is created by calling |CreateCachedResource| of ResourceCache.
// ResourceCache can have observers and when a resource is loaded,
// ResourceCache would notify its observers. For example, a DOM Document might
// be an observer of ResourceCache.
template <typename CacheType>
class ResourceCache {
 public:
  typedef CachedResource<CacheType> CachedResourceType;
  typedef typename CacheType::ResourceType ResourceType;
  typedef typename CacheType::DecoderProviderType DecoderProviderType;

  ResourceCache(const std::string& name, uint32 cache_capacity,
                scoped_ptr<DecoderProviderType> decoder_provider,
                loader::FetcherFactory* fetcher_factory);

  // |CreateCachedResource| returns CachedResource. If the CachedResource is not
  // in |cached_resource_map_| or its resource is not in
  // |unreference_cached_resource_map_|, creates a CachedResource with a loader
  // for it. If the CachedResource is in the cache map, return the
  // CachedResource or wrap the resource if necessary.
  scoped_refptr<CachedResourceType> CreateCachedResource(const GURL& url);

  // Set a callback that the loader will query to determine if the URL is safe
  // according to our document's security policy.
  void set_security_callback(const csp::SecurityCallback& security_callback) {
    security_callback_ = security_callback;
  }
  const csp::SecurityCallback& security_callback() const {
    return security_callback_;
  }

 private:
  friend class CachedResource<CacheType>;

  typedef base::hash_map<std::string, CachedResourceType*> CachedResourceMap;
  typedef typename CachedResourceMap::iterator CachedResourceMapIterator;

  typedef base::linked_hash_map<std::string, scoped_refptr<ResourceType> >
      ResourceMap;
  typedef typename ResourceMap::iterator ResourceMapIterator;

  // Called by CachedResource objects after they are successfully loaded.
  void NotifyResourceSuccessfullyLoaded(CachedResourceType* cached_resource);

  // Called by the destructor of CachedResource to remove CachedResource from
  // |cached_resource_map_| and either immediately free the resource from memory
  // or add it to |unreference_cached_resource_map_|, depending on whether the
  // cache is over its memory limit.
  void NotifyResourceDestroyed(CachedResourceType* cached_resource);

  void ReclaimMemory();

  // The name of this resource cache object, useful while debugging.
  const std::string name_;

  const uint32 cache_capacity_;

  scoped_ptr<DecoderProviderType> decoder_provider_;
  loader::FetcherFactory* const fetcher_factory_;
  csp::SecurityCallback security_callback_;

  // |cached_resource_map_| stores the cached resources that are currently
  // referenced.
  CachedResourceMap cached_resource_map_;

  // |unreference_cached_resource_map_| stores the cached resources that are
  // not referenced, but are being kept in memory as a result of the cache being
  // under its memory limit.
  ResourceMap unreference_cached_resource_map_;

  base::ThreadChecker resource_cache_thread_checker_;

  base::PublicCVal<uint32> size_in_bytes_;
  base::PublicCVal<uint32> capacity_in_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCache);
};

//////////////////////////////////////////////////////////////////////////
// ResourceCache - Definitions
//////////////////////////////////////////////////////////////////////////

template <typename CacheType>
ResourceCache<CacheType>::ResourceCache(
    const std::string& name, uint32 cache_capacity,
    scoped_ptr<DecoderProviderType> decoder_provider,
    loader::FetcherFactory* fetcher_factory)
    : name_(name),
      cache_capacity_(cache_capacity),
      decoder_provider_(decoder_provider.release()),
      fetcher_factory_(fetcher_factory),
      size_in_bytes_(base::StringPrintf("%s.Used", name_.c_str()), 0,
                     "Total number of bytes currently used by the cache."),
      capacity_in_bytes_(base::StringPrintf("%s.Capacity", name_.c_str()),
                         cache_capacity_,
                         "The capacity, in bytes, of the resource cache.  "
                         "Exceeding this results in *unused* resources being "
                         "purged.") {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  DCHECK(decoder_provider_.get());
  DCHECK(fetcher_factory_);
}

template <typename CacheType>
scoped_refptr<CachedResource<CacheType> >
ResourceCache<CacheType>::CreateCachedResource(const GURL& url) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  DCHECK(url.is_valid());

  // Try to find the resource from |cached_resource_map_|.
  CachedResourceMapIterator cached_resource_iterator =
      cached_resource_map_.find(url.spec());
  if (cached_resource_iterator != cached_resource_map_.end()) {
    return cached_resource_iterator->second;
  }

  // Try to find the resource from |unreference_cached_resource_map_|.
  ResourceMapIterator resource_iterator =
      unreference_cached_resource_map_.find(url.spec());
  if (resource_iterator != unreference_cached_resource_map_.end()) {
    scoped_refptr<CachedResourceType> cached_resource(
        new CachedResourceType(url, resource_iterator->second, this));
    cached_resource_map_.insert(
        std::make_pair(url.spec(), cached_resource.get()));
    unreference_cached_resource_map_.erase(url.spec());
    return cached_resource;
  }

  // If the resource doesn't exist, create a cached resource and fetch the
  // resource based on the url.
  scoped_refptr<CachedResourceType> cached_resource(
      new CachedResourceType(url, security_callback_, decoder_provider_.get(),
                             fetcher_factory_, this));
  cached_resource_map_.insert(
      std::make_pair(url.spec(), cached_resource.get()));
  return cached_resource;
}

template <typename CacheType>
void ResourceCache<CacheType>::NotifyResourceSuccessfullyLoaded(
    CachedResourceType* cached_resource) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());

  if (cached_resource->TryGetResource()) {
    size_in_bytes_ +=
        CacheType::GetEstimatedSizeInBytes(cached_resource->TryGetResource());
    if (size_in_bytes_ > cache_capacity_) {
      ReclaimMemory();
    }
  }
}

template <typename CacheType>
void ResourceCache<CacheType>::NotifyResourceDestroyed(
    CachedResourceType* cached_resource) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());

  std::string url = cached_resource->url().spec();
  cached_resource_map_.erase(url);

  DCHECK(unreference_cached_resource_map_.find(url) ==
         unreference_cached_resource_map_.end());
  // Check to see if this was a loaded resource.
  if (cached_resource->TryGetResource()) {
    // Add it into the unreferenced cached resource map, so that it will be
    // retained while memory is available for it in the cache.
    unreference_cached_resource_map_.insert(
        std::make_pair(url, cached_resource->TryGetResource()));
    // Try to reclaim some memory.
    ReclaimMemory();
  }
}

template <typename CacheType>
void ResourceCache<CacheType>::ReclaimMemory() {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());

  while (size_in_bytes_ > cache_capacity_ &&
         !unreference_cached_resource_map_.empty()) {
    // The first element is the earliest-inserted element.
    scoped_refptr<ResourceType> resource =
        unreference_cached_resource_map_.begin()->second;
    uint32 first_resource_size = resource->GetEstimatedSizeInBytes();
    // Erase the earliest-inserted element.
    // TODO: Erasing the earliest-inserted element could be a function
    // in linked_hash_map. Adding this function and related unit test which are
    // tracked in b/23790397.
    unreference_cached_resource_map_.erase(
        unreference_cached_resource_map_.begin());
    size_in_bytes_ -= first_resource_size;
  }

  // Make sure that |size_in_bytes_| is less than or equal to |cache_capacity_|,
  // otherwise it means that |unreference_cached_resource_map_| is empty. We
  // have to increase the size of |cache_capacity_| if the system memory is
  // large enough or evict resources from the cache even though they are still
  // in use.
  DLOG_IF(WARNING, size_in_bytes_ > cache_capacity_)
      << "cached size: " << size_in_bytes_
      << ", cache capacity: " << cache_capacity_;
}

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_RESOURCE_CACHE_H_
