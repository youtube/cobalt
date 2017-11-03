// Copyright 2015 Google Inc. All Rights Reserved.
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
//   static uint32 GetEstimatedSizeInBytes(
//       const scoped_refptr<ResourceType>& resource);

template <typename CacheType>
class ResourceCache;

enum CallbackType {
  kOnLoadingSuccessCallbackType,
  kOnLoadingErrorCallbackType,
  kCallbackTypeCount,
};

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

  typedef base::Callback<scoped_ptr<Loader>(
      const GURL&, const csp::SecurityCallback&,
      const base::Callback<void(const scoped_refptr<ResourceType>&)>&,
      const base::Callback<void(const std::string&)>&, const Origin&)>
      CreateLoaderFunction;

  // This class can be used to attach success or error callbacks to
  // CachedResource objects that are executed when the resource finishes
  // loading.
  // The callbacks are removed when the object is destroyed. If the resource has
  // already been loaded, execute the callback immediately.
  class OnLoadedCallbackHandler {
   public:
    OnLoadedCallbackHandler(
        const scoped_refptr<CachedResource>& cached_resource,
        const base::Closure& success_callback,
        const base::Closure& error_callback);
    ~OnLoadedCallbackHandler();

   private:
    typedef std::list<base::Closure>::iterator CallbackListIterator;

    scoped_refptr<CachedResource> cached_resource_;
    base::Closure success_callback_;
    base::Closure error_callback_;

    CallbackListIterator success_callback_list_iterator_;
    CallbackListIterator error_callback_list_iterator_;

    DISALLOW_COPY_AND_ASSIGN(OnLoadedCallbackHandler);
  };

  // Request fetching and decoding a single resource based on the url.
  CachedResource(const GURL& url,
                 const csp::SecurityCallback& security_callback,
                 const CreateLoaderFunction& create_loader_function,
                 ResourceCacheType* resource_cache, const Origin& origin);

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
  friend class ResourceCache<CacheType>;

  typedef std::list<base::Closure> CallbackList;
  typedef std::list<base::Closure>::iterator CallbackListIterator;

  ~CachedResource();

  // Callbacks for decoders.
  //
  // Notify that the resource is loaded successfully.
  void OnLoadingSuccess(const scoped_refptr<ResourceType>& resource);
  // Notify the loading error.
  void OnLoadingError(const std::string& error);

  // Called by |CachedResourceLoadedCallbackHandler|.
  CallbackListIterator AddCallback(CallbackType type,
                                   const base::Closure& callback);
  void RemoveCallback(CallbackType type, CallbackListIterator iterator);

  void RunCallbacks(CallbackType type);

  void EnableCompletionCallbacks();

  const GURL url_;

  scoped_refptr<ResourceType> resource_;
  ResourceCacheType* const resource_cache_;
  scoped_ptr<Loader> loader_;

  CallbackList callback_lists[kCallbackTypeCount];

  base::ThreadChecker cached_resource_thread_checker_;

  // In some cases (such as when the resource input data is stored in memory),
  // completion callbacks (e.g. resource fetch success/failure) could be
  // triggered from within the resource initialization callstack, and we are
  // not prepared to handle that. These members let us ensure that we are fully
  // initialized before we proceed with any completion callbacks.
  bool completion_callbacks_enabled_;
  base::Closure completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(CachedResource);
};

//////////////////////////////////////////////////////////////////////////
// CachedResource::OnLoadedCallbackHandler - Definitions
//////////////////////////////////////////////////////////////////////////

template <typename CacheType>
CachedResource<CacheType>::OnLoadedCallbackHandler::OnLoadedCallbackHandler(
    const scoped_refptr<CachedResource>& cached_resource,
    const base::Closure& success_callback,
    const base::Closure& error_callback)
    : cached_resource_(cached_resource),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  DCHECK(cached_resource_);

  if (!success_callback_.is_null()) {
    success_callback_list_iterator_ = cached_resource_->AddCallback(
        kOnLoadingSuccessCallbackType, success_callback_);
    if (cached_resource_->TryGetResource()) {
      success_callback_.Run();
    }
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
    const CreateLoaderFunction& create_loader_function,
    ResourceCacheType* resource_cache, const Origin& origin)
    : url_(url),
      resource_cache_(resource_cache),
      completion_callbacks_enabled_(false) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  loader_ = create_loader_function.Run(
      url, security_callback,
      base::Bind(&CachedResource::OnLoadingSuccess, base::Unretained(this)),
      base::Bind(&CachedResource::OnLoadingError, base::Unretained(this)),
      origin);
}

template <typename CacheType>
CachedResource<CacheType>::CachedResource(const GURL& url,
                                          ResourceType* resource,
                                          ResourceCacheType* resource_cache)
    : url_(url),
      resource_(resource),
      resource_cache_(resource_cache),
      completion_callbacks_enabled_(false) {
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

  completion_callback_ =
      base::Bind(&ResourceCacheType::NotifyResourceLoadingComplete,
                 base::Unretained(resource_cache_), base::Unretained(this),
                 kOnLoadingSuccessCallbackType);
  if (completion_callbacks_enabled_) {
    completion_callback_.Run();
  }
}

template <typename CacheType>
void CachedResource<CacheType>::OnLoadingError(const std::string& error) {
  DCHECK(cached_resource_thread_checker_.CalledOnValidThread());

  LOG(WARNING) << "Error while loading '" << url_ << "': " << error;

  loader_.reset();
  completion_callback_ =
      base::Bind(&ResourceCacheType::NotifyResourceLoadingComplete,
                 base::Unretained(resource_cache_), base::Unretained(this),
                 kOnLoadingErrorCallbackType);
  if (completion_callbacks_enabled_) {
    completion_callback_.Run();
  }
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

  // To avoid the list getting altered in the callbacks.
  CallbackList callback_list = callback_lists[type];
  CallbackListIterator callback_iter;
  for (callback_iter = callback_list.begin();
       callback_iter != callback_list.end(); ++callback_iter) {
    callback_iter->Run();
  }
}

template <typename CacheType>
void CachedResource<CacheType>::EnableCompletionCallbacks() {
  completion_callbacks_enabled_ = true;
  if (!completion_callback_.is_null()) {
    completion_callback_.Run();
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
      const base::Closure& error_callback)
      : cached_resource_(cached_resource),
        cached_resource_loaded_callback_handler_(
            new CachedResourceTypeOnLoadedCallbackHandler(
                cached_resource, success_callback, error_callback)) {}

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

  typedef
      typename CachedResourceType::CreateLoaderFunction CreateLoaderFunction;

  struct ResourceCallbackInfo {
    ResourceCallbackInfo(CachedResourceType* cached_resource,
                         CallbackType callback_type)
        : cached_resource(cached_resource), callback_type(callback_type) {}

    CachedResourceType* cached_resource;
    CallbackType callback_type;
  };

  ResourceCache(const std::string& name, uint32 cache_capacity,
                const CreateLoaderFunction& create_loader_function);

  // |CreateCachedResource| returns CachedResource. If the CachedResource is not
  // in |cached_resource_map_| or its resource is not in
  // |unreference_cached_resource_map_|, creates a CachedResource with a loader
  // for it. If the CachedResource is in the cache map, return the
  // CachedResource or wrap the resource if necessary.
  scoped_refptr<CachedResourceType> CreateCachedResource(const GURL& url,
                                                         const Origin& origin);

  // Set a callback that the loader will query to determine if the URL is safe
  // according to our document's security policy.
  void set_security_callback(const csp::SecurityCallback& security_callback) {
    security_callback_ = security_callback;
  }
  const csp::SecurityCallback& security_callback() const {
    return security_callback_;
  }

  uint32 capacity() const { return cache_capacity_; }
  void SetCapacity(uint32 capacity);

  void Purge();

  // Processes all pending callbacks regardless of the state of
  // |callback_blocking_loading_resource_set_|.
  void ProcessPendingCallbacks();

  void DisableCallbacks();

 private:
  friend class CachedResource<CacheType>;

  typedef base::hash_map<std::string, CachedResourceType*> CachedResourceMap;
  typedef typename CachedResourceMap::iterator CachedResourceMapIterator;

  typedef base::hash_set<std::string> ResourceSet;
  typedef base::linked_hash_map<std::string, ResourceCallbackInfo>
      ResourceCallbackMap;

  typedef base::linked_hash_map<std::string, scoped_refptr<ResourceType> >
      ResourceMap;
  typedef typename ResourceMap::iterator ResourceMapIterator;

  // Called by CachedResource objects after they finish loading.
  void NotifyResourceLoadingComplete(CachedResourceType* cached_resource,
                                     CallbackType callback_type);

  // Called by the destructor of CachedResource to remove CachedResource from
  // |cached_resource_map_| and either immediately free the resource from memory
  // or add it to |unreference_cached_resource_map_|, depending on whether the
  // cache is over its memory limit.
  void NotifyResourceDestroyed(CachedResourceType* cached_resource);

  // Reclaims memory from unreferenced cache objects until total cache memory
  // is reduced to |bytes_to_reclaim_down_to|. In the case where the desired
  // memory cannot be freed, pending callbacks are processed (potentially
  // enabling additional resources to be reclaimed), and memory reclamation is
  // attempted again.
  void ReclaimMemoryAndMaybeProcessPendingCallbacks(
      uint32 bytes_to_reclaim_down_to);
  // Releases unreferenced cache objects until our total cache memory usage is
  // less than or equal to |bytes_to_reclaim_down_to|, or until there are no
  // more unreferenced cache objects to release.
  void ReclaimMemory(uint32 bytes_to_reclaim_down_to, bool log_warning_if_over);

  // Calls ProcessPendingCallbacks() if
  // |callback_blocking_loading_resource_set_| is empty.
  void ProcessPendingCallbacksIfUnblocked();

  // The name of this resource cache object, useful while debugging.
  const std::string name_;

  uint32 cache_capacity_;

  CreateLoaderFunction create_loader_function_;

  csp::SecurityCallback security_callback_;

  // The resource cache attempts to batch callbacks as much as possible to try
  // to ensure that events triggered by the callbacks occur together. It
  // accomplishes this by waiting for all active loads to complete before
  // processing any of their callbacks. However, to ensure that callbacks are
  // processed in a timely manner as well, active loads are placed into two
  // buckets: callback blocking and non-callback blocking. While no callbacks
  // are pending, all active loads are added as callback blocking. As soon as
  // a callback is pending, any additional load requests are added as
  // non-callback blocking. As soon as all of the callback blocking loads are
  // finished, the pending callbacks are processed, the non-callback blocking
  // loads become callback blocking loads, and the process repeats itself.

  // Currently loading resources that block any pending callbacks from running.
  ResourceSet callback_blocking_loading_resource_set_;
  // Currently loading resources that do not block the pending callbacks from
  // running. After pending callbacks run, these become blocking.
  ResourceSet non_callback_blocking_loading_resource_set_;
  // Resources that have completed loading and have callbacks pending.
  ResourceCallbackMap pending_callback_map_;
  // Whether or not ProcessPendingCallbacks() is running.
  bool is_processing_pending_callbacks_;
  // Whether or not callbacks are currently disabled.
  bool are_callbacks_disabled_;

  // |cached_resource_map_| stores the cached resources that are currently
  // referenced.
  CachedResourceMap cached_resource_map_;

  // |unreference_cached_resource_map_| stores the cached resources that are
  // not referenced, but are being kept in memory as a result of the cache being
  // under its memory limit.
  ResourceMap unreference_cached_resource_map_;

  base::ThreadChecker resource_cache_thread_checker_;

  base::CVal<base::cval::SizeInBytes, base::CValPublic> memory_size_in_bytes_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      memory_capacity_in_bytes_;
  base::CVal<base::cval::SizeInBytes> memory_resources_loaded_in_bytes_;

  base::CVal<int> count_resources_requested_;
  base::CVal<int> count_resources_loading_;
  base::CVal<int> count_resources_loaded_;
  base::CVal<int> count_pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCache);
};

//////////////////////////////////////////////////////////////////////////
// ResourceCache - Definitions
//////////////////////////////////////////////////////////////////////////

template <typename CacheType>
ResourceCache<CacheType>::ResourceCache(
    const std::string& name, uint32 cache_capacity,
    const CreateLoaderFunction& create_loader_function)
    : name_(name),
      cache_capacity_(cache_capacity),
      create_loader_function_(create_loader_function),
      is_processing_pending_callbacks_(false),
      are_callbacks_disabled_(false),
      memory_size_in_bytes_(
          base::StringPrintf("Memory.%s.Size", name_.c_str()), 0,
          "Total number of bytes currently used by the cache."),
      memory_capacity_in_bytes_(
          base::StringPrintf("Memory.%s.Capacity", name_.c_str()),
          cache_capacity_,
          "The capacity, in bytes, of the resource cache.  "
          "Exceeding this results in *unused* resources being "
          "purged."),
      memory_resources_loaded_in_bytes_(
          base::StringPrintf("Memory.%s.Resource.Loaded", name_.c_str()), 0,
          "Combined size in bytes of all resources that have been loaded by "
          "the cache."),
      count_resources_requested_(
          base::StringPrintf("Count.%s.Resource.Requested", name_.c_str()), 0,
          "The total number of resources that have been requested."),
      count_resources_loading_(
          base::StringPrintf("Count.%s.Resource.Loading", name_.c_str()), 0,
          "The number of resources that are currently loading."),
      count_resources_loaded_(
          base::StringPrintf("Count.%s.Resource.Loaded", name_.c_str()), 0,
          "The total number of resources that have been successfully loaded."),
      count_pending_callbacks_(
          base::StringPrintf("Count.%s.PendingCallbacks", name_.c_str()), 0,
          "The number of loading completed resources that have pending "
          "callbacks.") {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  DCHECK(!create_loader_function_.is_null());
}

template <typename CacheType>
scoped_refptr<CachedResource<CacheType> >
ResourceCache<CacheType>::CreateCachedResource(const GURL& url,
                                               const Origin& origin) {
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

  // If we reach this point, then the resource doesn't exist yet.
  ++count_resources_requested_;

  // Add the resource to a loading set. If no current resources have pending
  // callbacks, then this resource will block callbacks until it is decoded.
  // However, if there are resources with pending callbacks, then the decoding
  // of this resource won't block the callbacks from occurring. This ensures
  // that a steady stream of new resources won't prevent callbacks from ever
  // occurring.
  if (pending_callback_map_.empty()) {
    callback_blocking_loading_resource_set_.insert(url.spec());
  } else {
    non_callback_blocking_loading_resource_set_.insert(url.spec());
  }
  ++count_resources_loading_;

  // Create the cached resource and fetch its resource based on the url.
  scoped_refptr<CachedResourceType> cached_resource(new CachedResourceType(
      url, security_callback_, create_loader_function_, this, origin));
  cached_resource_map_.insert(
      std::make_pair(url.spec(), cached_resource.get()));

  // Only now that we are finished initializing |cached_resource|, allow
  // completion callbacks to proceed. This can be an issue for resources that
  // load and decode synchronously and immediately.
  cached_resource->EnableCompletionCallbacks();

  return cached_resource;
}

template <typename CacheType>
void ResourceCache<CacheType>::SetCapacity(uint32 capacity) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  cache_capacity_ = capacity;
  memory_capacity_in_bytes_ = capacity;
  ReclaimMemoryAndMaybeProcessPendingCallbacks(cache_capacity_);
}

template <typename CacheType>
void ResourceCache<CacheType>::Purge() {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  ProcessPendingCallbacks();
  ReclaimMemory(0, true);
}

template <typename CacheType>
void ResourceCache<CacheType>::ProcessPendingCallbacks() {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());

  // If callbacks are disabled, simply return.
  if (are_callbacks_disabled_) {
    return;
  }

  is_processing_pending_callbacks_ = true;
  while (!pending_callback_map_.empty()) {
    ResourceCallbackInfo& callback_info = pending_callback_map_.front().second;

    // To avoid the last reference of this object getting deleted in the
    // callbacks.
    scoped_refptr<CachedResourceType> holder(callback_info.cached_resource);
    callback_info.cached_resource->RunCallbacks(callback_info.callback_type);

    pending_callback_map_.erase(pending_callback_map_.begin());
  }
  is_processing_pending_callbacks_ = false;
  count_pending_callbacks_ = 0;
}

template <typename CacheType>
void ResourceCache<CacheType>::DisableCallbacks() {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  are_callbacks_disabled_ = true;
}

template <typename CacheType>
void ResourceCache<CacheType>::NotifyResourceLoadingComplete(
    CachedResourceType* cached_resource, CallbackType callback_type) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  const std::string& url = cached_resource->url().spec();

  if (cached_resource->TryGetResource()) {
    uint32 estimated_size_in_bytes =
        CacheType::GetEstimatedSizeInBytes(cached_resource->TryGetResource());
    memory_size_in_bytes_ += estimated_size_in_bytes;
    memory_resources_loaded_in_bytes_ += estimated_size_in_bytes;

    ++count_resources_loaded_;
  }

  // Remove the resource from its loading set. It should exist in exactly one
  // of the loading sets.
  if (callback_blocking_loading_resource_set_.erase(url)) {
    DCHECK(non_callback_blocking_loading_resource_set_.find(url) ==
           non_callback_blocking_loading_resource_set_.end());
  } else if (!non_callback_blocking_loading_resource_set_.erase(url)) {
    DCHECK(false);
  }

  // Add a callback for the resource that just finished loading to the pending
  // callbacks.
  pending_callback_map_.insert(std::make_pair(
      url, ResourceCallbackInfo(cached_resource, callback_type)));

  // Update the loading resources and pending callbacks count. The callbacks are
  // incremented first to ensure that the total of the two counts always remains
  // above 0.
  ++count_pending_callbacks_;
  --count_resources_loading_;

  ProcessPendingCallbacksIfUnblocked();
  ReclaimMemoryAndMaybeProcessPendingCallbacks(cache_capacity_);
}

template <typename CacheType>
void ResourceCache<CacheType>::NotifyResourceDestroyed(
    CachedResourceType* cached_resource) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());
  const std::string& url = cached_resource->url().spec();

  cached_resource_map_.erase(url);

  DCHECK(unreference_cached_resource_map_.find(url) ==
         unreference_cached_resource_map_.end());

  // Check to see if this was a loaded resource.
  if (cached_resource->TryGetResource()) {
    // Add it into the unreferenced cached resource map, so that it will be
    // retained while memory is available for it in the cache.
    unreference_cached_resource_map_.insert(
        std::make_pair(url, cached_resource->TryGetResource()));
  }

  // Remove the resource from any loading or pending container that it is in.
  // It should never exist in more than one of the containers.
  if (callback_blocking_loading_resource_set_.erase(url)) {
    DCHECK(non_callback_blocking_loading_resource_set_.find(url) ==
           non_callback_blocking_loading_resource_set_.end());
    DCHECK(pending_callback_map_.find(url) == pending_callback_map_.end());
    --count_resources_loading_;
  } else if (non_callback_blocking_loading_resource_set_.erase(url)) {
    DCHECK(pending_callback_map_.find(url) == pending_callback_map_.end());
    --count_resources_loading_;
  } else if (pending_callback_map_.erase(url)) {
    --count_pending_callbacks_;
  }

  // Only process pending callbacks and attempt to reclaim memory if
  // NotifyResourceDestroyed() wasn't called from within
  // ProcessPendingCallbacks(). This prevents recursion and redundant
  // processing.
  if (!is_processing_pending_callbacks_) {
    ProcessPendingCallbacksIfUnblocked();
    ReclaimMemory(cache_capacity_, true /*log_warning_if_over*/);
  }
}

template <typename CacheType>
void ResourceCache<CacheType>::ReclaimMemoryAndMaybeProcessPendingCallbacks(
    uint32 bytes_to_reclaim_down_to) {
  ReclaimMemory(bytes_to_reclaim_down_to, false /*log_warning_if_over*/);
  // If the current size of the cache is still greater than
  // |bytes_to_reclaim_down_to| after reclaiming memory, then process any
  // pending callbacks and try again. References to the cached resources are
  // potentially being held until the callbacks run, so processing them may
  // enable more memory to be reclaimed.
  if (memory_size_in_bytes_ > bytes_to_reclaim_down_to) {
    ProcessPendingCallbacks();
    ReclaimMemory(bytes_to_reclaim_down_to, true /*log_warning_if_over*/);
  }
}

template <typename CacheType>
void ResourceCache<CacheType>::ReclaimMemory(uint32 bytes_to_reclaim_down_to,
                                             bool log_warning_if_over) {
  DCHECK(resource_cache_thread_checker_.CalledOnValidThread());

  while (memory_size_in_bytes_ > bytes_to_reclaim_down_to &&
         !unreference_cached_resource_map_.empty()) {
    // The first element is the earliest-inserted element.
    scoped_refptr<ResourceType> resource =
        unreference_cached_resource_map_.begin()->second;
    uint32 first_resource_size = resource->GetEstimatedSizeInBytes();
    // Erase the earliest-inserted element.
    // TODO: Erasing the earliest-inserted element could be a function
    // in linked_hash_map. Add that function and related unit test.
    unreference_cached_resource_map_.erase(
        unreference_cached_resource_map_.begin());
    memory_size_in_bytes_ -= first_resource_size;
  }

  if (log_warning_if_over) {
    // Log a warning if we're still over |bytes_to_reclaim_down_to| after
    // attempting to reclaim memory. This can occur validly when the size of
    // the referenced images exceeds the target size.
    DLOG_IF(WARNING, memory_size_in_bytes_ > bytes_to_reclaim_down_to)
        << "cached size: " << memory_size_in_bytes_
        << ", target size: " << bytes_to_reclaim_down_to;
  }
}

template <typename CacheType>
void ResourceCache<CacheType>::ProcessPendingCallbacksIfUnblocked() {
  if (callback_blocking_loading_resource_set_.empty()) {
    ProcessPendingCallbacks();

    // Now that we've processed the callbacks, if there are any non-blocking
    // loading resources, then they're becoming blocking. Simply swap the two
    // sets, rather than copying the contents over.
    if (!non_callback_blocking_loading_resource_set_.empty()) {
      callback_blocking_loading_resource_set_.swap(
          non_callback_blocking_loading_resource_set_);
    }
  }
}

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_RESOURCE_CACHE_H_
