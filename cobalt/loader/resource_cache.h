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

#ifndef COBALT_LOADER_RESOURCE_CACHE_H_
#define COBALT_LOADER_RESOURCE_CACHE_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "cobalt/base/c_val.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "net/base/linked_hash_map.h"
#include "url/gurl.h"

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

class CachedResourceBase
    : public base::RefCountedThreadSafe<CachedResourceBase> {
 public:
  // This class can be used to attach success or error callbacks to
  // CachedResource objects that are executed when the resource finishes
  // loading.
  // The callbacks are removed when the object is destroyed. If the resource
  // has already been loaded, execute the callback immediately.
  class OnLoadedCallbackHandler {
   public:
    OnLoadedCallbackHandler(
        const scoped_refptr<CachedResourceBase>& cached_resource,
        const base::Closure& success_callback,
        const base::Closure& error_callback);
    ~OnLoadedCallbackHandler();

   private:
    typedef std::list<base::Closure>::iterator CallbackListIterator;

    scoped_refptr<CachedResourceBase> cached_resource_;
    base::Closure success_callback_;
    base::Closure error_callback_;

    CallbackListIterator success_callback_list_iterator_;
    CallbackListIterator error_callback_list_iterator_;

    DISALLOW_COPY_AND_ASSIGN(OnLoadedCallbackHandler);
  };

  const GURL& url() const { return url_; }
  const Origin& origin() const { return origin_; }

  // Whether not the resource located at |url_| is finished loading.
  bool IsLoadingComplete();

 protected:
  friend class ResourceCacheBase;
  friend class base::RefCountedThreadSafe<CachedResourceBase>;
  friend class OnLoadedCallbackHandler;

  typedef std::list<base::Closure> CallbackList;
  typedef CallbackList::iterator CallbackListIterator;
  typedef base::Callback<std::unique_ptr<Loader>()> StartLoadingFunc;

  CachedResourceBase(
      const GURL& url, const StartLoadingFunc& start_loading_func,
      const base::Closure& on_retry_loading,
      const base::Callback<bool()>& has_resource_func,
      const base::Callback<void()>& reset_resource_func,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CallbackType)>& on_resource_loaded)
      : url_(url),
        start_loading_func_(start_loading_func),
        on_retry_loading_(on_retry_loading),
        has_resource_func_(has_resource_func),
        reset_resource_func_(reset_resource_func),
        are_loading_retries_enabled_func_(are_loading_retries_enabled_func),
        on_resource_loaded_(on_resource_loaded) {
    DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  }

  CachedResourceBase(
      const GURL& url, const Origin& origin,
      const StartLoadingFunc& start_loading_func,
      const base::Closure& on_retry_loading,
      const base::Callback<bool()>& has_resource_func,
      const base::Callback<void()>& reset_resource_func,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CallbackType)>& on_resource_loaded)
      : url_(url),
        origin_(origin),
        start_loading_func_(start_loading_func),
        on_retry_loading_(on_retry_loading),
        has_resource_func_(has_resource_func),
        reset_resource_func_(reset_resource_func),
        are_loading_retries_enabled_func_(are_loading_retries_enabled_func),
        on_resource_loaded_(on_resource_loaded) {
    DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
    StartLoading();
  }

  virtual ~CachedResourceBase();

  // Called by |CachedResourceLoadedCallbackHandler|.
  CallbackListIterator AddCallback(CallbackType callback_type,
                                   const base::Closure& callback);
  void RemoveCallback(CallbackType callback_type,
                      CallbackListIterator iterator);
  void RunCallbacks(CallbackType callback_type);
  void EnableCompletionCallbacks();

  // Start loading the resource located at |url_|. This encompasses both
  // fetching and decoding it.
  void StartLoading();

  // Schedule a loading retry on the resource located at |url_|. While there is
  // no limit on the number of retry attempts that can occur, the retry
  // scheduling uses an exponential backoff. The wait time doubles with each
  // subsequent attempt until a maximum wait time of 1024 seconds (~17 minutes)
  // is reached.
  void ScheduleLoadingRetry();

  // Notify the loading error.
  void OnLoadingComplete(const base::Optional<std::string>& error);

  THREAD_CHECKER(cached_resource_thread_checker_);

  const GURL url_;
  const Origin origin_;
  const StartLoadingFunc start_loading_func_;
  const base::Closure on_retry_loading_;
  const base::Callback<bool()> has_resource_func_;
  const base::Callback<void()> reset_resource_func_;
  const base::Callback<bool()> are_loading_retries_enabled_func_;
  const base::Callback<void(CallbackType)> on_resource_loaded_;

  std::unique_ptr<Loader> loader_;

  CallbackList callback_lists_[kCallbackTypeCount];

  // In some cases (such as when the resource input data is stored in memory),
  // completion callbacks (e.g. resource fetch success/failure) could be
  // triggered from within the resource initialization callstack, and we are
  // not prepared to handle that. These members let us ensure that we are fully
  // initialized before we proceed with any completion callbacks.
  bool are_completion_callbacks_enabled_ = false;
  base::Closure completion_callback_;

  // When the resource cache is set to allow retries and a transient loading
  // error causes a resource to fail to load, a retry is scheduled.
  int retry_count_ = 0;
  std::unique_ptr<base::RetainingOneShotTimer> retry_timer_;
};

// CachedResource requests fetching and decoding a single resource and the
// decoded resource is stored in |resource_|. CachedResource is created by
// calling |CreateCachedResource| of the ResourceCache.
template <typename CacheType>
class CachedResource : public CachedResourceBase {
 public:
  typedef typename CacheType::ResourceType ResourceType;

  typedef base::Callback<std::unique_ptr<Loader>(
      const GURL&, const Origin&, const csp::SecurityCallback&,
      const base::Callback<void(const scoped_refptr<ResourceType>&)>&,
      const base::Callback<void(const base::Optional<std::string>&)>&)>
      CreateLoaderFunction;

  // Request fetching and decoding a single resource based on the url.
  CachedResource(
      const GURL& url, const Origin& origin,
      const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
          start_loading_func,
      const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
      const base::Callback<void(CachedResource*)>& on_resource_destroyed,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CachedResource*, CallbackType)>&
          on_resource_loaded);

  // Resource is available. CachedResource is a wrapper of the resource
  // and there is no need to fetch or load this resource again. |loader_|
  // is NULL in this case.
  CachedResource(
      const GURL& url, ResourceType* resource,
      const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
          start_loading_func,
      const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
      const base::Callback<void(CachedResource*)>& on_resource_destroyed,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CachedResource*, CallbackType)>&
          on_resource_loaded);

  ~CachedResource() { on_resource_destroyed_.Run(this); }

  // If the resource is available in the cache, simply returns the resource. If
  // the resource loader is in loading status or encounters an error, still
  // returns |resource_| even if it is NULL to indicate no resource is
  // available.
  scoped_refptr<ResourceType> TryGetResource();

 private:
  friend class ResourceCache<CacheType>;

  // Callbacks for decoders.
  //
  // Notify that the resource is loaded successfully.
  void OnContentProduced(const scoped_refptr<ResourceType>& resource);

  bool HasResource() const {
    DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
    return resource_ != nullptr;
  }
  void ResetResource() {
    DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
    resource_ = nullptr;
  }

  const base::Callback<void(CachedResource*)> on_resource_destroyed_;
  scoped_refptr<ResourceType> resource_;

  DISALLOW_COPY_AND_ASSIGN(CachedResource);
};

template <typename CacheType>
CachedResource<CacheType>::CachedResource(
    const GURL& url, const Origin& origin,
    const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
        start_loading_func,
    const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
    const base::Callback<void(CachedResource*)>& on_resource_destroyed,
    const base::Callback<bool()>& are_loading_retries_enabled_func,
    const base::Callback<void(CachedResource*, CallbackType)>&
        on_resource_loaded)
    : CachedResourceBase(
          url, origin, base::Bind(start_loading_func, base::Unretained(this)),
          base::Bind(on_retry_loading, base::Unretained(this)),
          base::Bind(&CachedResource::HasResource, base::Unretained(this)),
          base::Bind(&CachedResource::ResetResource, base::Unretained(this)),
          are_loading_retries_enabled_func,
          base::Bind(on_resource_loaded, base::Unretained(this))),
      on_resource_destroyed_(on_resource_destroyed) {}

template <typename CacheType>
CachedResource<CacheType>::CachedResource(
    const GURL& url, ResourceType* resource,
    const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
        start_loading_func,
    const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
    const base::Callback<void(CachedResource*)>& on_resource_destroyed,
    const base::Callback<bool()>& are_loading_retries_enabled_func,
    const base::Callback<void(CachedResource*, CallbackType)>&
        on_resource_loaded)
    : CachedResourceBase(
          url, base::Bind(start_loading_func, base::Unretained(this)),
          base::Bind(on_retry_loading, base::Unretained(this)),
          base::Bind(&CachedResource::HasResource, base::Unretained(this)),
          base::Bind(&CachedResource::ResetResource, base::Unretained(this)),
          are_loading_retries_enabled_func,
          base::Bind(on_resource_loaded, base::Unretained(this))),
      on_resource_destroyed_(on_resource_destroyed),
      resource_(resource) {}

template <typename CacheType>
scoped_refptr<typename CacheType::ResourceType>
CachedResource<CacheType>::TryGetResource() {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  return resource_;
}

template <typename CacheType>
void CachedResource<CacheType>::OnContentProduced(
    const scoped_refptr<ResourceType>& resource) {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  DCHECK(!resource_);

  resource_ = resource;
}

template <typename CacheType>
class CachedResourceReferenceWithCallbacks {
 public:
  typedef CachedResource<CacheType> CachedResourceType;
  typedef typename CachedResourceType::OnLoadedCallbackHandler
      CachedResourceTypeOnLoadedCallbackHandler;

  typedef std::vector<std::unique_ptr<CachedResourceReferenceWithCallbacks>>
      CachedResourceReferenceVector;

  CachedResourceReferenceWithCallbacks(
      const scoped_refptr<CachedResourceType>& cached_resource,
      const base::Closure& content_produced_callback,
      const base::Closure& load_complete_callback)
      : cached_resource_(cached_resource),
        cached_resource_loaded_callback_handler_(
            new CachedResourceTypeOnLoadedCallbackHandler(
                cached_resource, content_produced_callback,
                load_complete_callback)) {}

  scoped_refptr<CachedResourceType> cached_resource() {
    return cached_resource_;
  }

 private:
  // A single cached resource.
  scoped_refptr<CachedResourceType> cached_resource_;
  // This handles adding and removing the resource loaded callbacks.
  std::unique_ptr<CachedResourceTypeOnLoadedCallbackHandler>
      cached_resource_loaded_callback_handler_;
};

class ResourceCacheBase {
 public:
  typedef base::Callback<void(uint32 bytes_to_reclaim_down_to,
                              bool log_warning_if_over)>
      ReclaimMemoryFunc;
  ResourceCacheBase(const std::string& name, uint32 cache_capacity,
                    bool are_loading_retries_enabled,
                    const ReclaimMemoryFunc& reclaim_memory_func);

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

 protected:
  struct ResourceCallbackInfo {
    ResourceCallbackInfo(CachedResourceBase* cached_resource,
                         CallbackType callback_type)
        : cached_resource(cached_resource), callback_type(callback_type) {}

    CachedResourceBase* cached_resource;
    CallbackType callback_type;
  };

  typedef base::hash_set<std::string> ResourceSet;
  typedef net::linked_hash_map<std::string, ResourceCallbackInfo>
      ResourceCallbackMap;

  // Called by CachedResource objects when they fail to load as a result of a
  // transient error and are scheduling a retry.
  void NotifyResourceLoadingRetryScheduled(CachedResourceBase* cached_resource);

  // Reclaims memory from unreferenced cache objects until total cache memory
  // is reduced to |bytes_to_reclaim_down_to|. In the case where the desired
  // memory cannot be freed, pending callbacks are processed (potentially
  // enabling additional resources to be reclaimed), and memory reclamation is
  // attempted again.
  void ReclaimMemoryAndMaybeProcessPendingCallbacks(
      uint32 bytes_to_reclaim_down_to);

  // Calls ProcessPendingCallbacks() if
  // |callback_blocking_loading_resource_set_| is empty.
  void ProcessPendingCallbacksIfUnblocked();

  bool are_loading_retries_enabled() const {
    return are_loading_retries_enabled_;
  }

  THREAD_CHECKER(resource_cache_thread_checker_);

  // The name of this resource cache object, useful while debugging.
  const std::string name_;

  bool are_loading_retries_enabled_;

  uint32 cache_capacity_;

  const ReclaimMemoryFunc reclaim_memory_func_;

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
  // Timer used to ensure that pending callbacks are handled in a timely manner
  // when callbacks are being blocked by additional loading resources.
  base::OneShotTimer process_pending_callback_timer_;

  // Whether or not ProcessPendingCallbacks() is running.
  bool is_processing_pending_callbacks_ = false;
  // Whether or not callbacks are currently disabled.
  bool are_callbacks_disabled_ = false;

  base::CVal<base::cval::SizeInBytes, base::CValPublic> memory_size_in_bytes_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      memory_capacity_in_bytes_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      memory_resources_loaded_in_bytes_;

  base::CVal<int, base::CValPublic> count_resources_requested_;
  base::CVal<int, base::CValPublic> count_resources_loading_;
  base::CVal<int, base::CValPublic> count_resources_loaded_;
  base::CVal<int, base::CValPublic> count_resources_cached_;
  base::CVal<int, base::CValPublic> count_pending_callbacks_;
};

// CachedResource is created by calling |CreateCachedResource| of ResourceCache.
// ResourceCache can have observers and when a resource is loaded,
// ResourceCache would notify its observers. For example, a DOM Document might
// be an observer of ResourceCache.
template <typename CacheType>
class ResourceCache : public ResourceCacheBase {
 public:
  typedef CachedResource<CacheType> CachedResourceType;
  typedef typename CacheType::ResourceType ResourceType;

  typedef
      typename CachedResourceType::CreateLoaderFunction CreateLoaderFunction;

  ResourceCache(const std::string& name, uint32 cache_capacity,
                bool are_load_retries_enabled,
                const CreateLoaderFunction& create_loader_function);

  // |CreateCachedResource| returns CachedResource. If the CachedResource is not
  // in |cached_resource_map_| or its resource is not in
  // |unreference_cached_resource_map_|, creates a CachedResource with a loader
  // for it. If the CachedResource is in the cache map, return the
  // CachedResource or wrap the resource if necessary.
  scoped_refptr<CachedResourceType> CreateCachedResource(const GURL& url,
                                                         const Origin& origin);

 private:
  typedef base::hash_map<std::string, CachedResourceType*> CachedResourceMap;
  typedef typename CachedResourceMap::iterator CachedResourceMapIterator;

  typedef net::linked_hash_map<std::string, scoped_refptr<ResourceType>>
      ResourceMap;
  typedef typename ResourceMap::iterator ResourceMapIterator;

  std::unique_ptr<Loader> StartLoadingResource(
      CachedResourceType* cached_resource);

  // Called by CachedResource objects after they finish loading.
  void NotifyResourceLoadingComplete(CachedResourceType* cached_resource,
                                     CallbackType callback_type);

  // Called by the destructor of CachedResource to remove CachedResource from
  // |cached_resource_map_| and either immediately free the resource from memory
  // or add it to |unreference_cached_resource_map_|, depending on whether the
  // cache is over its memory limit.
  void NotifyResourceDestroyed(CachedResourceType* cached_resource);

  // Releases unreferenced cache objects until our total cache memory usage is
  // less than or equal to |bytes_to_reclaim_down_to|, or until there are no
  // more unreferenced cache objects to release.
  void ReclaimMemory(uint32 bytes_to_reclaim_down_to, bool log_warning_if_over);

  const CreateLoaderFunction create_loader_function_;

  // |cached_resource_map_| stores the cached resources that are currently
  // referenced.
  CachedResourceMap cached_resource_map_;

  // |unreference_cached_resource_map_| stores the cached resources that are
  // not referenced, but are being kept in memory as a result of the cache being
  // under its memory limit.
  ResourceMap unreference_cached_resource_map_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCache);
};

template <typename CacheType>
ResourceCache<CacheType>::ResourceCache(
    const std::string& name, uint32 cache_capacity,
    bool are_loading_retries_enabled,
    const CreateLoaderFunction& create_loader_function)
    : ResourceCacheBase(
          name, cache_capacity, are_loading_retries_enabled,
          base::Bind(&ResourceCache::ReclaimMemory, base::Unretained(this))),
      create_loader_function_(create_loader_function) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  DCHECK(!create_loader_function_.is_null());
}

template <typename CacheType>
scoped_refptr<CachedResource<CacheType>>
ResourceCache<CacheType>::CreateCachedResource(const GURL& url,
                                               const Origin& origin) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
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
    scoped_refptr<CachedResourceType> cached_resource(new CachedResourceType(
        url, resource_iterator->second.get(),
        base::Bind(&ResourceCache::StartLoadingResource,
                   base::Unretained(this)),
        base::Bind(&ResourceCache::NotifyResourceLoadingRetryScheduled,
                   base::Unretained(this)),
        base::Bind(&ResourceCache::NotifyResourceDestroyed,
                   base::Unretained(this)),
        base::Bind(&ResourceCache::are_loading_retries_enabled,
                   base::Unretained(this)),
        base::Bind(&ResourceCache::NotifyResourceLoadingComplete,
                   base::Unretained(this))));
    cached_resource_map_.insert(
        std::make_pair(url.spec(), cached_resource.get()));
    unreference_cached_resource_map_.erase(url.spec());
    return cached_resource;
  }

  // If we reach this point, then the resource doesn't exist yet.
  ++count_resources_requested_;

  // Create the cached resource and fetch its resource based on the url.
  scoped_refptr<CachedResourceType> cached_resource(new CachedResourceType(
      url, origin,
      base::Bind(&ResourceCache::StartLoadingResource, base::Unretained(this)),
      base::Bind(&ResourceCache::NotifyResourceLoadingRetryScheduled,
                 base::Unretained(this)),
      base::Bind(&ResourceCache::NotifyResourceDestroyed,
                 base::Unretained(this)),
      base::Bind(&ResourceCache::are_loading_retries_enabled,
                 base::Unretained(this)),
      base::Bind(&ResourceCache::NotifyResourceLoadingComplete,
                 base::Unretained(this))));
  cached_resource_map_.insert(
      std::make_pair(url.spec(), cached_resource.get()));

  // Only now that we are finished initializing |cached_resource|, allow
  // completion callbacks to proceed. This can be an issue for resources that
  // load and decode synchronously and immediately.
  cached_resource->EnableCompletionCallbacks();

  return cached_resource;
}

template <typename CacheType>
std::unique_ptr<Loader> ResourceCache<CacheType>::StartLoadingResource(
    CachedResourceType* cached_resource) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  const std::string& url = cached_resource->url().spec();

  // The resource should not already be in either of the loading sets.
  DCHECK(callback_blocking_loading_resource_set_.find(url) ==
         callback_blocking_loading_resource_set_.end());
  DCHECK(non_callback_blocking_loading_resource_set_.find(url) ==
         non_callback_blocking_loading_resource_set_.end());

  // Add the resource to a loading set. If no current resources have pending
  // callbacks, then this resource will block callbacks until it is decoded.
  // However, if there are resources with pending callbacks, then the decoding
  // of this resource won't block the callbacks from occurring. This ensures
  // that a steady stream of new resources won't prevent callbacks from ever
  // occurring.
  if (pending_callback_map_.empty()) {
    callback_blocking_loading_resource_set_.insert(url);
  } else {
    non_callback_blocking_loading_resource_set_.insert(url);
  }

  ++count_resources_loading_;

  return create_loader_function_.Run(
      cached_resource->url(), cached_resource->origin(), security_callback_,
      base::Bind(&CachedResourceType::OnContentProduced,
                 base::Unretained(cached_resource)),
      base::Bind(&CachedResourceType::OnLoadingComplete,
                 base::Unretained(cached_resource)));
}

template <typename CacheType>
void ResourceCache<CacheType>::NotifyResourceLoadingComplete(
    CachedResourceType* cached_resource, CallbackType callback_type) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  const std::string& url = cached_resource->url().spec();

  if (cached_resource->TryGetResource()) {
    uint32 estimated_size_in_bytes =
        CacheType::GetEstimatedSizeInBytes(cached_resource->TryGetResource());
    memory_size_in_bytes_ += estimated_size_in_bytes;
    memory_resources_loaded_in_bytes_ += estimated_size_in_bytes;

    ++count_resources_loaded_;
    ++count_resources_cached_;
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
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
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
void ResourceCache<CacheType>::ReclaimMemory(uint32 bytes_to_reclaim_down_to,
                                             bool log_warning_if_over) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);

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
    --count_resources_cached_;
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

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_RESOURCE_CACHE_H_
