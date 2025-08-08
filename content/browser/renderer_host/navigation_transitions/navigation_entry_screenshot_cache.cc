// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const base::FeatureParam<bool> kDumpWithoutCrashNavigationEntryScreenshotCache{
    &blink::features::kBackForwardTransitions,
    "dump-without-crash-navigation-entry-screenshot-cache", false};

NavigationEntryScreenshotCache::CompressedCallback& GetTestCallback() {
  static base::NoDestructor<NavigationEntryScreenshotCache::CompressedCallback>
      instance;
  return *instance;
}

std::unique_ptr<NavigationEntryScreenshot> RemoveScreenshotFromEntry(
    NavigationEntry* entry) {
  CHECK(entry);
  std::unique_ptr<base::SupportsUserData::Data> data =
      entry->TakeUserData(NavigationEntryScreenshot::kUserDataKey);
  CHECK(data);
  auto* screenshot = static_cast<NavigationEntryScreenshot*>(data.release());
  CHECK(screenshot->is_cached());
  screenshot->SetCache(nullptr);
  return base::WrapUnique(screenshot);
}

}  // namespace

// static
void NavigationEntryScreenshotCache::SetCompressedCallbackForTesting(
    CompressedCallback callback) {
  GetTestCallback() = std::move(callback);
}

NavigationEntryScreenshotCache::NavigationEntryScreenshotCache(
    base::SafeRef<NavigationEntryScreenshotManager> manager,
    NavigationControllerImpl* nav_controller)
    : manager_(manager), nav_controller_(nav_controller) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(NavigationTransitionConfig::AreBackForwardTransitionsEnabled());
}

NavigationEntryScreenshotCache::~NavigationEntryScreenshotCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PurgeInternal(/*reason=*/std::nullopt);
}

void NavigationEntryScreenshotCache::SetScreenshot(
    base::WeakPtr<NavigationRequest> navigation_request,
    std::unique_ptr<NavigationEntryScreenshot> screenshot,
    bool is_copied_from_embedder) {
  if (!navigation_request) {
    SetScreenshotInternal(std::move(screenshot), is_copied_from_embedder);
    return;
  }

  const int64_t navigation_id = navigation_request->GetNavigationId();
  CHECK(!pending_screenshots_.contains(navigation_id));
  PendingScreenshot pending_screenshot(std::move(screenshot),
                                       is_copied_from_embedder);
  pending_screenshots_[navigation_id] = std::move(pending_screenshot);
}

void NavigationEntryScreenshotCache::OnNavigationFinished(
    const NavigationRequest& navigation_request) {
  auto it = pending_screenshots_.find(navigation_request.GetNavigationId());
  if (it == pending_screenshots_.end()) {
    if (!navigation_request.HasCommitted()) {
      // crbug.com/369200379: If the navigation fails to commit and the
      // screenshot hasn't arrived at the browser yet, we need to increment the
      // copy output request sequence on the screenshot destination entry to
      // prevent the the screenshot eventually being stashed. Since the
      // navigation never commits, it's erroneous to stash this screenshot into
      // the last committed entry.
      nav_controller_->GetLastCommittedEntry()
          ->navigation_transition_data()
          .increment_copy_output_request_sequence();
    }
    return;
  }

  if (!navigation_request.HasCommitted()) {
    pending_screenshots_.erase(it);
    return;
  }

  SetScreenshotInternal(std::move(it->second.screenshot),
                        it->second.is_copied_from_embedder);
  pending_screenshots_.erase(it);
}

void NavigationEntryScreenshotCache::SetVisible(bool visible) {
  const bool currently_visible = !last_visible_timestamp_.has_value();
  if (visible == currently_visible) {
    return;
  }

  if (visible) {
    last_visible_timestamp_.reset();
  } else {
    last_visible_timestamp_ = manager_->Now();
  }

  manager_->OnVisibilityChanged(this);
}

void NavigationEntryScreenshotCache::SetScreenshotInternal(
    std::unique_ptr<NavigationEntryScreenshot> screenshot,
    bool is_copied_from_embedder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int index =
      NavigationTransitionUtils::FindEntryIndexForNavigationTransitionID(
          nav_controller_, screenshot->unique_id());
  NavigationEntryImpl* entry = nav_controller_->GetEntryAtIndex(index);
  if (!entry) {
    // The entry was deleted by the time we received the bitmap from the GPU.
    // This can happen by clearing the session history, or when the
    // `NavigationEntry` was replaced or deleted, etc.
    return;
  }

  // Should never capture the last committed entry.
  if (entry == nav_controller_->GetLastCommittedEntry()) {
    if (kDumpWithoutCrashNavigationEntryScreenshotCache.Get()) {
      SCOPED_CRASH_KEY_BOOL("dnt", "is_copied_from_embedder",
                            is_copied_from_embedder);
      base::debug::DumpWithoutCrashing();
    }
    return;
  }

  // A navigation entry without a screenshot will be removed from the cache
  // first (thus not tracked). Impossible to overwrite for a cached entry.
  // TODO(crbug.com/373893401): Find out why this happens.
  if (entry->GetUserData(NavigationEntryScreenshot::kUserDataKey)) {
    if (kDumpWithoutCrashNavigationEntryScreenshotCache.Get()) {
      base::debug::DumpWithoutCrashing();
    }
    RemoveScreenshot(entry);
  }

  auto& transition_data = entry->navigation_transition_data();
  CHECK(cached_screenshots_.find(transition_data.unique_id()) ==
        cached_screenshots_.end());
  CHECK(!screenshot->is_cached());
  const size_t size = screenshot->SetCache(this);

  entry->SetUserData(NavigationEntryScreenshot::kUserDataKey,
                     std::move(screenshot));
  transition_data.set_is_copied_from_embedder(is_copied_from_embedder);
  transition_data.SetSameDocumentNavigationEntryScreenshotToken(std::nullopt);
  transition_data.set_cache_hit_or_miss_reason(
      NavigationTransitionData::CacheHitOrMissReason::kCacheHit);
  // Tentative fix for crbug.com/373893401.
  transition_data.increment_copy_output_request_sequence();

  cached_screenshots_[transition_data.unique_id()] = size;
  manager_->OnScreenshotCached(this, size);
}

std::unique_ptr<NavigationEntryScreenshot>
NavigationEntryScreenshotCache::RemoveScreenshot(
    NavigationEntry* navigation_entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(navigation_entry);
  auto it = cached_screenshots_.find(
      static_cast<NavigationEntryImpl*>(navigation_entry)
          ->navigation_transition_data()
          .unique_id());
  // `CHECK_NE` is not compatible with `base::flat_set`.
  CHECK(it != cached_screenshots_.end());

  // Remove the tracked nav entry id and the entry and update the metadata.
  const size_t size = it->second;
  cached_screenshots_.erase(it);
  auto screenshot = RemoveScreenshotFromEntry(navigation_entry);
  static_cast<NavigationEntryImpl*>(navigation_entry)
      ->navigation_transition_data()
      .set_cache_hit_or_miss_reason(std::nullopt);
  manager_->OnScreenshotRemoved(this, size);

  return screenshot;
}

void NavigationEntryScreenshotCache::OnNavigationEntryGone(
    NavigationTransitionData::UniqueId screenshot_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = cached_screenshots_.find(screenshot_id);
  CHECK(it != cached_screenshots_.end());

  const size_t size = it->second;
  cached_screenshots_.erase(it);
  manager_->OnScreenshotRemoved(this, size);
}

void NavigationEntryScreenshotCache::OnScreenshotCompressed(
    NavigationTransitionData::UniqueId screenshot_id,
    size_t new_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = cached_screenshots_.find(screenshot_id);
  CHECK(it != cached_screenshots_.end());

  const size_t old_size = it->second;
  it->second = new_size;
  manager_->OnScreenshotCompressed(this, old_size, new_size);

  int index =
      NavigationTransitionUtils::FindEntryIndexForNavigationTransitionID(
          nav_controller_, screenshot_id);

  if (GetTestCallback()) {
    std::move(GetTestCallback()).Run(index);
  }
}

void NavigationEntryScreenshotCache::EvictScreenshotsUntilUnderBudgetOrEmpty() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK(!IsEmpty());

  CHECK_GT(manager_->GetCurrentCacheSize(), manager_->GetMaxCacheSize());

  const int current_index = nav_controller_->GetLastCommittedEntryIndex();
  NavigationTransitionData::UniqueId screenshot_id =
      nav_controller_->GetEntryAtIndex(current_index)
          ->navigation_transition_data()
          .unique_id();
  // It's impossible to have a screenshot for the current entry.
  CHECK(!cached_screenshots_.contains(screenshot_id));
  // Impossible to have just one entry (the current entry).
  CHECK_GT(nav_controller_->GetEntryCount(), 1);

  int distance_to_leftmost = current_index;
  int distance_to_rightmost =
      nav_controller_->GetEntryCount() - current_index - 1;

  // The eviction strategy is to priotrize keeping the screenshots for the
  // navigation entries that are closer to the "current entry" (last committed).
  // This strategy assumes the user is equally likely to go back/forward. This
  // is not true for Android where native OS gesture navigation only takes the
  // user back (even right-edge swipe).
  //
  // TODO(crbug.com/40256524): Iterate on the eviction strategy based on metrics
  // when this launches.
  //
  // Ex: [3, 4&, 5*, 6&, 7, 8&], where "*" means the last committed entry and
  // "&" means an entry with a screenshot. In this case `distance_to_leftmost` =
  // 2 and `distance_to_rightmost` = 3. The eviction order will be: entry8,
  // entry6 and entry4.
  //
  while (manager_->GetCurrentCacheSize() > manager_->GetMaxCacheSize() &&
         !IsEmpty()) {
    auto candidate_screenshot_id = NavigationTransitionData::kInvalidId;
    CHECK(distance_to_leftmost > 0 || distance_to_rightmost > 0);
    if (distance_to_leftmost > distance_to_rightmost) {
      candidate_screenshot_id =
          nav_controller_->GetEntryAtIndex(current_index - distance_to_leftmost)
              ->navigation_transition_data()
              .unique_id();
      --distance_to_leftmost;
    } else {
      candidate_screenshot_id =
          nav_controller_
              ->GetEntryAtIndex(current_index + distance_to_rightmost)
              ->navigation_transition_data()
              .unique_id();
      --distance_to_rightmost;
    }
    // Check whether this candidate entry has a screenshot to remove, or
    // continue to move closer to the current entry.
    int candidate_index =
        NavigationTransitionUtils::FindEntryIndexForNavigationTransitionID(
            nav_controller_, candidate_screenshot_id);
    auto* candidate_entry = nav_controller_->GetEntryAtIndex(candidate_index);
    CHECK(candidate_entry);
    if (auto it = cached_screenshots_.find(candidate_screenshot_id);
        it != cached_screenshots_.end()) {
      std::unique_ptr<NavigationEntryScreenshot> evicted_screenshot =
          RemoveScreenshotFromEntry(candidate_entry);
      candidate_entry->navigation_transition_data()
          .set_cache_hit_or_miss_reason(
              NavigationTransitionData::CacheHitOrMissReason::
                  kCacheMissEvicted);

      const size_t size = it->second;
      cached_screenshots_.erase(it);
      CHECK_LE(size, manager_->GetCurrentCacheSize());
      manager_->OnScreenshotRemoved(this, size);
    }
  }
}

void NavigationEntryScreenshotCache::Purge(PurgeReason reason) {
  PurgeInternal(reason);
}

void NavigationEntryScreenshotCache::PurgeInternal(
    std::optional<PurgeReason> reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = cached_screenshots_.begin();
  while (!IsEmpty()) {
    int evicted_index =
        NavigationTransitionUtils::FindEntryIndexForNavigationTransitionID(
            nav_controller_, it->first);
    auto* evicted_entry = nav_controller_->GetEntryAtIndex(evicted_index);
    CHECK(evicted_entry);
    auto purged = RemoveScreenshotFromEntry(evicted_entry);
    const size_t size = it->second;
    cached_screenshots_.erase(it);

    if (reason) {
      NavigationTransitionData::CacheHitOrMissReason metric_reason;
      switch (*reason) {
        case PurgeReason::kMemoryPressure:
          metric_reason = NavigationTransitionData::CacheHitOrMissReason::
              kCacheMissPurgedMemoryPressure;
          break;
        case PurgeReason::kInvisible:
          metric_reason = NavigationTransitionData::CacheHitOrMissReason::
              kCacheMissInvisible;
          break;
      }
      evicted_entry->navigation_transition_data().set_cache_hit_or_miss_reason(
          metric_reason);
    } else {
      // Resetting the UMA enum since at this point `this` is getting destroyed
      // by the destructor which invalidates the enum value.
      evicted_entry->navigation_transition_data().set_cache_hit_or_miss_reason(
          std::nullopt);
    }

    CHECK_LE(size, manager_->GetCurrentCacheSize());
    manager_->OnScreenshotRemoved(this, size);
    it = cached_screenshots_.begin();
  }
}

bool NavigationEntryScreenshotCache::IsEmpty() const {
  return cached_screenshots_.empty();
}

std::optional<base::TimeTicks>
NavigationEntryScreenshotCache::GetLastVisibleTime() const {
  return last_visible_timestamp_;
}

NavigationEntryScreenshotCache::PendingScreenshot::PendingScreenshot() =
    default;
NavigationEntryScreenshotCache::PendingScreenshot::PendingScreenshot(
    std::unique_ptr<NavigationEntryScreenshot> screenshot,
    bool is_copied_from_embedder)
    : screenshot(std::move(screenshot)),
      is_copied_from_embedder(is_copied_from_embedder) {}
NavigationEntryScreenshotCache::PendingScreenshot::~PendingScreenshot() =
    default;
NavigationEntryScreenshotCache::PendingScreenshot::PendingScreenshot(
    PendingScreenshot&& other) = default;
NavigationEntryScreenshotCache::PendingScreenshot&
NavigationEntryScreenshotCache::PendingScreenshot::operator=(
    PendingScreenshot&& other) = default;

}  // namespace content
