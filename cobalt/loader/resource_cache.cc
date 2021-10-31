// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/resource_cache.h"

#include <algorithm>
#include <memory>

#include "base/strings/stringprintf.h"
#include "cobalt/base/console_log.h"

namespace cobalt {
namespace loader {

CachedResourceBase::OnLoadedCallbackHandler::OnLoadedCallbackHandler(
    const scoped_refptr<CachedResourceBase>& cached_resource,
    const base::Closure& success_callback,
    const base::Closure& error_callback)
    : cached_resource_(cached_resource),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  DCHECK(cached_resource_);

  if (!success_callback_.is_null()) {
    success_callback_list_iterator_ = cached_resource_->AddCallback(
        kOnLoadingSuccessCallbackType, success_callback_);
    if (cached_resource_->has_resource_func_.Run()) {
      success_callback_.Run();
    }
  }

  if (!error_callback_.is_null()) {
    error_callback_list_iterator_ = cached_resource_->AddCallback(
        kOnLoadingErrorCallbackType, error_callback_);
  }
}

net::LoadTimingInfo
    CachedResourceBase::OnLoadedCallbackHandler::GetLoadTimingInfo() {
  return cached_resource_->GetLoadTimingInfo();
}

CachedResourceBase::OnLoadedCallbackHandler::~OnLoadedCallbackHandler() {
  if (!success_callback_.is_null()) {
    cached_resource_->RemoveCallback(kOnLoadingSuccessCallbackType,
                                     success_callback_list_iterator_);
  }

  if (!error_callback_.is_null()) {
    cached_resource_->RemoveCallback(kOnLoadingErrorCallbackType,
                                     error_callback_list_iterator_);
  }
}

bool CachedResourceBase::IsLoadingComplete() {
  return !loader_ && !retry_timer_;
}

CachedResourceBase::CallbackListIterator CachedResourceBase::AddCallback(
    CallbackType callback_type, const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);

  CallbackList& callback_list = callback_lists_[callback_type];
  callback_list.push_front(callback);
  return callback_list.begin();
}

void CachedResourceBase::RemoveCallback(CallbackType type,
                                        CallbackListIterator iterator) {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);

  CallbackList& callback_list = callback_lists_[type];
  callback_list.erase(iterator);
}

void CachedResourceBase::RunCallbacks(CallbackType type) {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);

  // To avoid the list getting altered in the callbacks.
  CallbackList callback_list = callback_lists_[type];
  CallbackListIterator callback_iter;
  for (callback_iter = callback_list.begin();
       callback_iter != callback_list.end(); ++callback_iter) {
    callback_iter->Run();
  }
}

void CachedResourceBase::EnableCompletionCallbacks() {
  are_completion_callbacks_enabled_ = true;
  if (!completion_callback_.is_null()) {
    completion_callback_.Run();
  }
}

void CachedResourceBase::StartLoading() {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  DCHECK(!loader_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());

  loader_ = start_loading_func_.Run();
}

void CachedResourceBase::ScheduleLoadingRetry() {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);
  DCHECK(!loader_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());

  LOG(WARNING) << "Scheduling loading retry for '" << url_ << "'";
  on_retry_loading_.Run();

  // The delay starts at 1 second and doubles every subsequent retry until the
  // maximum delay of 1024 seconds (~17 minutes) is reached. After this, all
  // additional attempts also wait 1024 seconds.
  const int64 kBaseRetryDelayInMilliseconds = 1000;
  const int kMaxRetryCountShift = 10;
  int64 delay = kBaseRetryDelayInMilliseconds
                << std::min(kMaxRetryCountShift, retry_count_++);

  // The retry timer is lazily created the first time that it is needed.
  if (!retry_timer_) {
    retry_timer_.reset(new base::RetainingOneShotTimer());
  }
  retry_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(delay),
      base::Bind(&CachedResourceBase::StartLoading, base::Unretained(this)));
}

void CachedResourceBase::OnLoadingComplete(
    const base::Optional<std::string>& error) {
  DCHECK_CALLED_ON_VALID_THREAD(cached_resource_thread_checker_);

  if (loader_ != nullptr) {
    load_timing_info_ = loader_->get_load_timing_info();
  }

  // Success
  if (!error) {
    loader_.reset();
    retry_timer_.reset();

    completion_callback_ =
        base::Bind(on_resource_loaded_, kOnLoadingSuccessCallbackType);
    if (are_completion_callbacks_enabled_) {
      completion_callback_.Run();
    }
    // Error
  } else {
    CLOG(WARNING, owner_->debugger_hooks())
        << " Error while loading '" << url_ << "': " << *error;

    if (has_resource_func_.Run()) {
      LOG(WARNING) << "A resource was produced but there was still an error.";
      reset_resource_func_.Run();
    }

    bool should_retry = are_loading_retries_enabled_func_.Run() &&
                        loader_->DidFailFromTransientError();

    loader_.reset();

    if (should_retry) {
      ScheduleLoadingRetry();
    } else {
      retry_timer_.reset();

      completion_callback_ =
          base::Bind(on_resource_loaded_, kOnLoadingErrorCallbackType);
      if (are_completion_callbacks_enabled_) {
        completion_callback_.Run();
      }
    }
  }
}

void ResourceCacheBase::SetCapacity(uint32 capacity) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  cache_capacity_ = capacity;
  memory_capacity_in_bytes_ = capacity;
  ReclaimMemoryAndMaybeProcessPendingCallbacks(cache_capacity_);
}

void ResourceCacheBase::Purge() {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  ProcessPendingCallbacks();
  reclaim_memory_func_.Run(0, true);
}

void ResourceCacheBase::ProcessPendingCallbacks() {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  process_pending_callback_timer_.Stop();

  // If callbacks are disabled, simply return.
  if (are_callbacks_disabled_) {
    return;
  }

  is_processing_pending_callbacks_ = true;
  while (!pending_callback_map_.empty()) {
    ResourceCallbackInfo& callback_info = pending_callback_map_.front().second;

    // To avoid the last reference of this object getting deleted in the
    // callbacks.
    auto* cached_resource_ptr = callback_info.cached_resource;
    scoped_refptr<CachedResourceBase> holder(cached_resource_ptr);
    callback_info.cached_resource->RunCallbacks(callback_info.callback_type);

    pending_callback_map_.erase(pending_callback_map_.begin());
  }
  is_processing_pending_callbacks_ = false;
  count_pending_callbacks_ = 0;
}

void ResourceCacheBase::DisableCallbacks() {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  are_callbacks_disabled_ = true;
}

ResourceCacheBase::ResourceCacheBase(
    const std::string& name, const base::DebuggerHooks& debugger_hooks,
    uint32 cache_capacity, bool are_loading_retries_enabled,
    const ReclaimMemoryFunc& reclaim_memory_func)
    : name_(name),
      debugger_hooks_(debugger_hooks),
      are_loading_retries_enabled_(are_loading_retries_enabled),
      cache_capacity_(cache_capacity),
      reclaim_memory_func_(reclaim_memory_func),
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
          "The total number of resources that have been successfully "
          "loaded."),
      count_resources_cached_(
          base::StringPrintf("Count.%s.Resource.Cached", name_.c_str()), 0,
          "The number of resources that are currently in the cache."),
      count_pending_callbacks_(
          base::StringPrintf("Count.%s.PendingCallbacks", name_.c_str()), 0,
          "The number of loading completed resources that have pending "
          "callbacks.") {}

void ResourceCacheBase::NotifyResourceLoadingRetryScheduled(
    CachedResourceBase* cached_resource) {
  DCHECK_CALLED_ON_VALID_THREAD(resource_cache_thread_checker_);
  const std::string& url = cached_resource->url().spec();

  // Remove the resource from those currently loading. It'll be re-added once
  // the retry starts.

  // Remove the resource from its loading set. It should exist in exactly one
  // of the loading sets.
  if (callback_blocking_loading_resource_set_.erase(url)) {
    DCHECK(non_callback_blocking_loading_resource_set_.find(url) ==
           non_callback_blocking_loading_resource_set_.end());
  } else if (!non_callback_blocking_loading_resource_set_.erase(url)) {
    DCHECK(false);
  }

  --count_resources_loading_;

  ProcessPendingCallbacksIfUnblocked();
}

void ResourceCacheBase::ReclaimMemoryAndMaybeProcessPendingCallbacks(
    uint32 bytes_to_reclaim_down_to) {
  reclaim_memory_func_.Run(bytes_to_reclaim_down_to,
                           false /*log_warning_if_over*/);
  // If the current size of the cache is still greater than
  // |bytes_to_reclaim_down_to| after reclaiming memory, then process any
  // pending callbacks and try again. References to the cached resources are
  // potentially being held until the callbacks run, so processing them may
  // enable more memory to be reclaimed.
  if (memory_size_in_bytes_ > bytes_to_reclaim_down_to) {
    ProcessPendingCallbacks();
    reclaim_memory_func_.Run(bytes_to_reclaim_down_to,
                             true /*log_warning_if_over*/);
  }
}

void ResourceCacheBase::ProcessPendingCallbacksIfUnblocked() {
  // If there are no callback blocking resources, then simply process any
  // pending callbacks now; otherwise, start |process_pending_callback_timer_|,
  // which ensures that the callbacks are handled in a timely manner while still
  // allowing them to be batched.
  if (callback_blocking_loading_resource_set_.empty()) {
    ProcessPendingCallbacks();

    // Now that we've processed the callbacks, if there are any non-blocking
    // loading resources, then they're becoming blocking. Simply swap the two
    // sets, rather than copying the contents over.
    if (!non_callback_blocking_loading_resource_set_.empty()) {
      callback_blocking_loading_resource_set_.swap(
          non_callback_blocking_loading_resource_set_);
    }
  } else if (!pending_callback_map_.empty() &&
             !process_pending_callback_timer_.IsRunning()) {
    // The maximum delay for a pending callback is set to 500ms. After that, the
    // callback will be processed regardless of how many callback blocking
    // loading resources remain. This specific value maximizes callback batching
    // on fast networks while also keeping the callback delay on slow networks
    // to a minimum and is based on significant testing.
    const int64 kMaxPendingCallbackDelayInMilliseconds = 500;
    process_pending_callback_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(
            kMaxPendingCallbackDelayInMilliseconds),
        base::BindOnce(&ResourceCacheBase::ProcessPendingCallbacks,
                       base::Unretained(this)));
  }
}

}  // namespace loader
}  // namespace cobalt
