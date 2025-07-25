// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_MANAGER_H_

#include "base/containers/lru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/safe_ref.h"
#include "content/common/content_export.h"

namespace content {

class NavigationEntryScreenshotCacheEvictor;

// This class manages the metadata for all `NavigationEntryScreenshot`s captured
// for the backward and forward navigation entries per `FrameTree`. The
// screenshots are used to present users with previews of the previous pages
// when the users initiate back/forward-navigations. This class is owned by a
// `BrowserContext`. All primary `FrameTree`s sharing the same `BrowserContext`
// share the same manager. The manager should only be accessed by the
// `NavigationEntryScreenshotCache` and tests.
class CONTENT_EXPORT NavigationEntryScreenshotManager {
 public:
  NavigationEntryScreenshotManager();
  NavigationEntryScreenshotManager(const NavigationEntryScreenshotManager&) =
      delete;
  NavigationEntryScreenshotManager& operator=(
      const NavigationEntryScreenshotManager&) = delete;
  ~NavigationEntryScreenshotManager();

  // Called when a screenshot is stashed into a `NavigationEntry`, or when a
  // screenshot is removed from the entry (for preview, or during the
  // destruction of the entry).
  void OnScreenshotCached(NavigationEntryScreenshotCacheEvictor* cache,
                          size_t size);
  void OnScreenshotRemoved(NavigationEntryScreenshotCacheEvictor* cache,
                           size_t size);

  // Called when a cache's owning `NavigationController` becomes visible. This
  // cache is the the most recently used cache.
  void OnCacheBecameVisible(NavigationEntryScreenshotCacheEvictor* cache);

  bool IsEmpty() const;

  size_t GetCurrentCacheSize() const { return current_cache_size_in_bytes_; }
  size_t GetMaxCacheSize() const { return max_cache_size_in_bytes_; }

  // Allow tests to customize memory budget.
  void SetMemoryBudgetForTesting(size_t size) {
    max_cache_size_in_bytes_ = size;
  }

  base::SafeRef<NavigationEntryScreenshotManager> GetSafeRef() const {
    return weak_factory_.GetSafeRef();
  }

 private:
  // Called when the first screenshot is cached into `cache`, and when the last
  // screenshot is removed from `cache`.
  void Register(NavigationEntryScreenshotCacheEvictor* cache);
  void Unregister(NavigationEntryScreenshotCacheEvictor* cache);

  // Called at the end of `OnScreenshotCached`.
  void EvictIfOutOfMemoryBudget();

  // Used by `listener_`. When the system memory is under critical pressure, all
  // screenshots under this `Profile` are purged.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
  FRIEND_TEST_ALL_PREFIXES(NavigationEntryScreenshotCacheTest,
                           OnMemoryPressureCritical);

  size_t max_cache_size_in_bytes_;
  size_t current_cache_size_in_bytes_ = 0U;

  // The `listener_` monitors the system memory pressure, and calls
  // `NavigationEntryScreenshotManager::OnMemoryPressure` when the system
  // memory pressure level changes.
  std::unique_ptr<base::MemoryPressureListener> listener_;

  // The most recently used cache is stored at the front of the
  // `base::LRUCacheSet`. A limited interface to the tab's cache is used so that
  // this BrowserContext-wide manager does not have access to details like URLs
  // or pixels within each tab.
  base::LRUCacheSet<NavigationEntryScreenshotCacheEvictor*> managed_caches_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NavigationEntryScreenshotManager> weak_factory_{this};
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_MANAGER_H_
