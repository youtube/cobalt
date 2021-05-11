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
#include "cobalt/base/debugger_hooks.h"
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

class ResourceCacheBase;

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

    net::LoadTimingInfo GetLoadTimingInfo();
    scoped_refptr<CachedResourceBase>& GetCachedResource() {
      return cached_resource_;
    }
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

  net::LoadTimingInfo GetLoadTimingInfo() {
    return load_timing_info_;
  }

  bool get_resource_timing_created_flag() {
    return is_resource_timing_created_flag_;
  }

  void set_resource_timing_created_flag(bool is_created) {
    is_resource_timing_created_flag_ = is_created;
  }

 protected:
  friend class ResourceCacheBase;
  friend class base::RefCountedThreadSafe<CachedResourceBase>;
  friend class OnLoadedCallbackHandler;

  typedef std::list<base::Closure> CallbackList;
  typedef CallbackList::iterator CallbackListIterator;
  typedef base::Callback<std::unique_ptr<Loader>()> StartLoadingFunc;

  CachedResourceBase(
      const ResourceCacheBase* owner, const GURL& url,
      const StartLoadingFunc& start_loading_func,
      const base::Closure& on_retry_loading,
      const base::Callback<bool()>& has_resource_func,
      const base::Callback<void()>& reset_resource_func,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CallbackType)>& on_resource_loaded)
      : owner_(owner),
        url_(url),
        start_loading_func_(start_loading_func),
        on_retry_loading_(on_retry_loading),
        has_resource_func_(has_resource_func),
        reset_resource_func_(reset_resource_func),
        are_loading_retries_enabled_func_(are_loading_retries_enabled_func),
        on_resource_loaded_(on_resource_loaded),
        is_resource_timing_created_flag_(false) {
    DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  }

  CachedResourceBase(
      const ResourceCacheBase* owner, const GURL& url, const Origin& origin,
      const StartLoadingFunc& start_loading_func,
      const base::Closure& on_retry_loading,
      const base::Callback<bool()>& has_resource_func,
      const base::Callback<void()>& reset_resource_func,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CallbackType)>& on_resource_loaded)
      : owner_(owner),
        url_(url),
        origin_(origin),
        start_loading_func_(start_loading_func),
        on_retry_loading_(on_retry_loading),
        has_resource_func_(has_resource_func),
        reset_resource_func_(reset_resource_func),
        are_loading_retries_enabled_func_(are_loading_retries_enabled_func),
        on_resource_loaded_(on_resource_loaded),
        is_resource_timing_created_flag_(false) {
    DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  }

  virtual ~CachedResourceBase() {}

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

  const ResourceCacheBase* owner_;
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

  net::LoadTimingInfo load_timing_info_;
  bool is_resource_timing_created_flag_;
};

// CachedResource requests fetching and decoding a single resource and the
// decoded resource is stored in |resource_|. CachedResource is created by
// calling |GetOrCreateCachedResource| of the ResourceCache.
template <typename CacheType>
class CachedResource : public CachedResourceBase {
 public:
  typedef typename CacheType::ResourceType ResourceType;

  // Request fetching and decoding a single resource based on the url.
  CachedResource(
      const ResourceCache<CacheType>* owner, const GURL& url,
      const Origin& origin,
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
      const ResourceCache<CacheType>* owner, const GURL& url,
      ResourceType* resource,
      const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
          start_loading_func,
      const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
      const base::Callback<void(CachedResource*)>& on_resource_destroyed,
      const base::Callback<bool()>& are_loading_retries_enabled_func,
      const base::Callback<void(CachedResource*, CallbackType)>&
          on_resource_loaded);

  ~CachedResource() override {
    if (retry_timer_) {
      retry_timer_->Stop();
    }

    on_resource_destroyed_.Run(this);

    for (int i = 0; i < kCallbackTypeCount; ++i) {
      DCHECK(callback_lists_[i].empty());
    }

    loader_.reset();
  }

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
    const ResourceCache<CacheType>* owner, const GURL& url,
    const Origin& origin,
    const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
        start_loading_func,
    const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
    const base::Callback<void(CachedResource*)>& on_resource_destroyed,
    const base::Callback<bool()>& are_loading_retries_enabled_func,
    const base::Callback<void(CachedResource*, CallbackType)>&
        on_resource_loaded)
    : CachedResourceBase(
          owner, url, origin,
          base::Bind(start_loading_func, base::Unretained(this)),
          base::Bind(on_retry_loading, base::Unretained(this)),
          base::Bind(&CachedResource::HasResource, base::Unretained(this)),
          base::Bind(&CachedResource::ResetResource, base::Unretained(this)),
          are_loading_retries_enabled_func,
          base::Bind(on_resource_loaded, base::Unretained(this))),
      on_resource_destroyed_(on_resource_destroyed) {
  StartLoading();
}

template <typename CacheType>
CachedResource<CacheType>::CachedResource(
    const ResourceCache<CacheType>* owner, const GURL& url,
    ResourceType* resource,
    const base::Callback<std::unique_ptr<Loader>(CachedResource*)>&
        start_loading_func,
    const base::Callback<void(CachedResourceBase*)>& on_retry_loading,
    const base::Callback<void(CachedResource*)>& on_resource_destroyed,
    const base::Callback<bool()>& are_loading_retries_enabled_func,
    const base::Callback<void(CachedResource*, CallbackType)>&
        on_resource_loaded)
    : CachedResourceBase(
          owner, url, base::Bind(start_loading_func, base::Unretained(this)),
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

// It is similar to CachedResource but doesn't hold a strong reference to the
// underlying resource, so the underlying resource can still be released during
// purging, after all unreferenced resources are released.
// It is created by calling |CreateWeakCachedResource|.
template <typename CacheType>
class WeakCachedResource {
 public:
  explicit WeakCachedResource(const base::Closure& on_resource_destroyed_cb)
      : on_resource_destroyed_cb_(on_resource_destroyed_cb) {
    DCHECK(!on_resource_destroyed_cb_.is_null());
  }
  ~WeakCachedResource() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    on_resource_destroyed_cb_.Run();
  }

 private:
  THREAD_CHECKER(thread_checker_);

  base::Closure on_resource_destroyed_cb_;
};

// TODO: Collapse this into OnLoadedCallbackHandler.
class CachedResourceReferenceWithCallbacks {
 public:
  // typedef CachedResource<CacheType> CachedResourceType;
  typedef typename CachedResourceBase::OnLoadedCallbackHandler
      CachedResourceTypeOnLoadedCallbackHandler;

  typedef std::vector<std::unique_ptr<CachedResourceReferenceWithCallbacks>>
      CachedResourceReferenceVector;

  CachedResourceReferenceWithCallbacks(
      const scoped_refptr<CachedResourceBase>& cached_resource,
      const base::Closure& content_produced_callback,
      const base::Closure& load_complete_callback)
      : cached_resource_loaded_callback_handler_(cached_resource,
                                                 content_produced_callback,
                                                 load_complete_callback) {}
  scoped_refptr<CachedResourceBase>& GetCachedResource() {
    return cached_resource_loaded_callback_handler_.GetCachedResource();
  }

 private:
  // This handles adding and removing the resource loaded callbacks.
  CachedResourceTypeOnLoadedCallbackHandler
      cached_resource_loaded_callback_handler_;
};

class ResourceCacheBase {
 public:
  typedef base::Callback<void(uint32 bytes_to_reclaim_down_to,
                              bool log_warning_if_over)>
      ReclaimMemoryFunc;

  // Set a callback that the loader will query to determine if the URL is safe
  // according to our document's security policy.
  void set_security_callback(const csp::SecurityCallback& security_callback) {
    security_callback_ = security_callback;
  }
  const csp::SecurityCallback& security_callback() const {
    return security_callback_;
  }

  const base::DebuggerHooks& debugger_hooks() const { return debugger_hooks_; }

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

  ResourceCacheBase(const std::string& name,
                    const base::DebuggerHooks& debugger_hooks,
                    uint32 cache_capacity, bool are_loading_retries_enabled,
                    const ReclaimMemoryFunc& reclaim_memory_func);

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

  const base::DebuggerHooks& debugger_hooks_;

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

// CachedResource is created by calling |GetOrCreateCachedResource| of
// ResourceCache.
// ResourceCache can have observers and when a resource is loaded,
// ResourceCache would notify its observers. For example, a DOM Document might
// be an observer of ResourceCache.
template <typename CacheType>
class ResourceCache : public ResourceCacheBase {
 public:
  typedef CachedResource<CacheType> CachedResourceType;
  typedef WeakCachedResource<CacheType> WeakCachedResourceType;
  typedef typename CacheType::ResourceType ResourceType;

  typedef base::Callback<std::unique_ptr<Loader>(
      const GURL&, const Origin&, const csp::SecurityCallback&,
      const base::Callback<void(const scoped_refptr<ResourceType>&)>&,
      const base::Callback<void(const base::Optional<std::string>&)>&)>
      CreateLoaderFunction;

  // Call this function to notify the caller that this resource is requested.
  typedef base::Callback<void(const std::string&)>
      NotifyResourceRequestedFunction;

  ResourceCache(const std::string& name,
                const base::DebuggerHooks& debugger_hooks,
                uint32 cache_capacity, bool are_loading_retries_enabled,
                const CreateLoaderFunction& create_loader_function,
                const NotifyResourceRequestedFunction&
                    notify_resource_requested_function =
                        NotifyResourceRequestedFunction());
  ~ResourceCache() {
    DCHECK(weak_cached_resource_ref_count_map_.empty());
    DCHECK(cached_resource_map_.empty());
  }

  // |GetOrCreateCachedResource| returns CachedResource. If the CachedResource
  // is not in |cached_resource_map_| or its resource is not in
  // |unreferenced_cached_resource_map_|, creates a CachedResource with a loader
  // for it. If the CachedResource is in the cache map, return the
  // CachedResource or wrap the resource if necessary.
  scoped_refptr<CachedResourceType> GetOrCreateCachedResource(
      const GURL& url, const Origin& origin);

  // |CreateWeakCachedResource| returns a WeakCachedResource referenced to the
  // same resource identified by the url of |cached_resource|.  A weak
  // referenced resource may still be released during purging, but only after
  // all unreferenced resources are released.
  std::unique_ptr<WeakCachedResourceType> CreateWeakCachedResource(
      const scoped_refptr<CachedResourceType>& cached_resource);

 private:
  typedef base::hash_map<std::string, CachedResourceType*> CachedResourceMap;
  typedef net::linked_hash_map<std::string, int> WeakCachedResourceRefCountMap;
  typedef net::linked_hash_map<std::string, scoped_refptr<ResourceType>>
      ResourceMap;

  std::unique_ptr<Loader> StartLoadingResource(
      CachedResourceType* cached_resource);

  // Called by CachedResource objects after they finish loading.
  void NotifyResourceLoadingComplete(CachedResourceType* cached_resource,
                                     CallbackType callback_type);

  // Called by the destructor of CachedResource to remove CachedResource from
  // |cached_resource_map_| and add it to |weak_referenced_cached_resource_map_|
  // or |unreferenced_cached_resource_map_|, depending on whether the resource
  // is still weakly referenced.
  // It will then start purging and may immediately free the resource from
  // memory ifthe cache is over its memory limit.
  void NotifyResourceDestroyed(CachedResourceType* cached_resource);

  // Called by the destructor of WeakCachedResource to remove WeakCachedResource
  // from |weak_cached_resource_ref_count_map_| and add it to
  // |unreferenced_cached_resource_map_|.
  void NotifyWeakResourceDestroyed(const std::string& url);

  // Releases unreferenced cache objects until our total cache memory usage is
  // less than or equal to |bytes_to_reclaim_down_to|, or until there are no
  // more unreferenced cache objects to release.
  void ReclaimMemory(uint32 bytes_to_reclaim_down_to, bool log_warning_if_over);

  const CreateLoaderFunction create_loader_function_;
  const NotifyResourceRequestedFunction notify_resource_requested_function_;

  // Stores the cached resources that are currently referenced.
  CachedResourceMap cached_resource_map_;

  // Stores the urls to the cached resources that are weakly referenced, with
  // their ref counts.
  WeakCachedResourceRefCountMap weak_cached_resource_ref_count_map_;

  // Stores the cached resources that are not referenced, but are being kept in
  // memory as a result of the cache being under its memory limit.
  ResourceMap unreferenced_cached_resource_map_;

  // Stores the cached resources that are weakly referenced, they will be
  // released during purging, once all resources in the above defined
  // |unreferenced_cached_resource_map_| are released.
  // While it could be great to sort the resources by both the reference counts
  // and the last usage, in reality all ref counts are 1 and we only need to put
  // new items at the end of the map.
  ResourceMap weak_referenced_cached_resource_map_;

  base::Callback<void(const net::LoadTimingInfo&)> load_timing_info_callback_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCache);
};

template <typename CacheType>
ResourceCache<CacheType>::ResourceCache(
    const std::string& name, const base::DebuggerHooks& debugger_hooks,
    uint32 cache_capacity, bool are_loading_retries_enabled,
    const CreateLoaderFunction& create_loader_function,
    const NotifyResourceRequestedFunction& notify_resource_requested_function)
    : ResourceCacheBase(
          name, debugger_hooks, cache_capacity, are_loading_retries_enabled,
          base::Bind(&ResourceCache::ReclaimMemory, base::Unretained(this))),
      create_loader_function_(create_loader_function),
      notify_resource_requested_function_(notify_resource_requested_function) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  DCHECK(!create_loader_function_.is_null());
}

template <typename CacheType>
scoped_refptr<CachedResource<CacheType>>
ResourceCache<CacheType>::GetOrCreateCachedResource(const GURL& url,
                                                    const Origin& origin) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  DCHECK(url.is_valid());

  // TODO: We should also notify the fetcher cache when the resource is
  // destroyed.
  if (!notify_resource_requested_function_.is_null()) {
    notify_resource_requested_function_.Run(url.spec());
  }

  // Try to find the resource from |cached_resource_map_|.
  auto cached_resource_iterator = cached_resource_map_.find(url.spec());
  if (cached_resource_iterator != cached_resource_map_.end()) {
    return cached_resource_iterator->second;
  }

  // Try to find the resource from |unreferenced_cached_resource_map_|.
  auto resource_iterator = unreferenced_cached_resource_map_.find(url.spec());
  if (resource_iterator != unreferenced_cached_resource_map_.end()) {
    scoped_refptr<CachedResourceType> cached_resource(new CachedResourceType(
        this, url, resource_iterator->second.get(),
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
    unreferenced_cached_resource_map_.erase(resource_iterator);
    return cached_resource;
  }

  // Try to find the resource from |weak_referenced_cached_resource_map_|.
  resource_iterator = weak_referenced_cached_resource_map_.find(url.spec());
  if (resource_iterator != weak_referenced_cached_resource_map_.end()) {
    scoped_refptr<CachedResourceType> cached_resource(new CachedResourceType(
        this, url, resource_iterator->second.get(),
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
    weak_referenced_cached_resource_map_.erase(resource_iterator);
    return cached_resource;
  }

  // If we reach this point, then the resource doesn't exist yet.
  ++count_resources_requested_;

  // Create the cached resource and fetch its resource based on the url.
  scoped_refptr<CachedResourceType> cached_resource(new CachedResourceType(
      this, url, origin,
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
std::unique_ptr<WeakCachedResource<CacheType>>
ResourceCache<CacheType>::CreateWeakCachedResource(
    const scoped_refptr<CachedResourceType>& cached_resource) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  DCHECK(cached_resource);

  auto url = cached_resource->url().spec();
  std::unique_ptr<WeakCachedResourceType> weak_cached_resource(
      new WeakCachedResourceType(
          base::Bind(&ResourceCache::NotifyWeakResourceDestroyed,
                     base::Unretained(this), url)));

  auto iterator = weak_cached_resource_ref_count_map_.find(url);
  int ref_count = 1;
  if (iterator != weak_cached_resource_ref_count_map_.end()) {
    ref_count = iterator->second + 1;
    weak_cached_resource_ref_count_map_.erase(iterator);
  }
  weak_cached_resource_ref_count_map_.insert(std::make_pair(url, ref_count));

  return weak_cached_resource;
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

  DCHECK(weak_referenced_cached_resource_map_.find(url) ==
         weak_referenced_cached_resource_map_.end());
  DCHECK(unreferenced_cached_resource_map_.find(url) ==
         unreferenced_cached_resource_map_.end());

  // Check to see if this was a loaded resource.
  if (cached_resource->TryGetResource()) {
    if (weak_cached_resource_ref_count_map_.find(url) !=
        weak_cached_resource_ref_count_map_.end()) {
      // Add it into the weak referenced cached resource map, so that it will be
      // retained while memory is available for it in the cache, and will be
      // purged after all unreferenced cached resources.
      weak_referenced_cached_resource_map_.insert(
          std::make_pair(url, cached_resource->TryGetResource()));
    } else {
      // Add it into the unreferenced cached resource map, so that it will be
      // retained while memory is available for it in the cache.
      unreferenced_cached_resource_map_.insert(
          std::make_pair(url, cached_resource->TryGetResource()));
    }
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
void ResourceCache<CacheType>::NotifyWeakResourceDestroyed(
    const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);

  auto iterator = weak_cached_resource_ref_count_map_.find(url);
  DCHECK(iterator != weak_cached_resource_ref_count_map_.end());
  if (iterator->second > 1) {
    --iterator->second;
    return;
  }

  weak_cached_resource_ref_count_map_.erase(iterator);
  auto resource_iterator = weak_referenced_cached_resource_map_.find(url);
  if (resource_iterator != weak_referenced_cached_resource_map_.end()) {
    unreferenced_cached_resource_map_.insert(
        std::make_pair(resource_iterator->first, resource_iterator->second));
    weak_referenced_cached_resource_map_.erase(resource_iterator);
  }
}

template <typename CacheType>
void ResourceCache<CacheType>::ReclaimMemory(uint32 bytes_to_reclaim_down_to,
                                             bool log_warning_if_over) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);

  ResourceMap* resource_maps[] = {&unreferenced_cached_resource_map_,
                                  &weak_referenced_cached_resource_map_};

  for (size_t i = 0; i < SB_ARRAY_SIZE(resource_maps); ++i) {
    while (memory_size_in_bytes_ > bytes_to_reclaim_down_to &&
           !resource_maps[i]->empty()) {
      // The first element is the earliest-inserted element.
      scoped_refptr<ResourceType> resource = resource_maps[i]->begin()->second;
      uint32 first_resource_size = resource->GetEstimatedSizeInBytes();
      // Erase the earliest-inserted element.
      // TODO: Erasing the earliest-inserted element could be a function
      // in linked_hash_map. Add that function and related unit test.
      resource_maps[i]->erase(resource_maps[i]->begin());
      memory_size_in_bytes_ -= first_resource_size;
      --count_resources_cached_;
    }
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
