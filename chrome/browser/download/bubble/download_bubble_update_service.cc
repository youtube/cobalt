// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_update_service.h"

#include <iterator>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/download_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using ::offline_items_collection::ContentId;
using ::offline_items_collection::OfflineContentProvider;
using ::offline_items_collection::OfflineItem;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;
using ItemSortKey = DownloadBubbleUpdateService::ItemSortKey;
template <typename Id, typename Item>
using IterMap = DownloadBubbleUpdateService::IterMap<Id, Item>;
using ProgressInfo = DownloadDisplayController::ProgressInfo;
template <typename Item>
using SortedItems = DownloadBubbleUpdateService::SortedItems<Item>;

// Show up to 30 items in total by default.
constexpr size_t kDefaultMaxNumItemsToShow = 30u;
// Cache a few more items of each type than we will return from
// GetAllModelsToDisplay. This gives us some wiggle room and makes it more
// likely that we'll return enough items before backfilling.
constexpr size_t kDefaultExtraItemsToCache = 30u;
// Amount of time to show an item in the bubble. Items older than this duration
// ago will be pruned.
constexpr base::TimeDelta kShowItemInBubbleDuration = base::Hours(24);
// Don't send the "download started" notification for an extension or theme
// (crx) download until 2 seconds after it has begun. If it is a small download
// that finishes in under 2 seconds, the download UI does not show at all. If it
// is a large download that takes longer than 2 seconds, show the UI so that the
// user knows Chrome is working on it.
constexpr base::TimeDelta kCrxShowNewItemDelay = base::Seconds(2);
// Limit the size of the |delayed_crx_guids_| set so it doesn't grow
// unboundedly. It is unlikely that the user would have 20 active crx downloads
// simultaneously.
constexpr int kMaxDelayedCrxGuids = 20;

template <typename Item>
ItemSortKey::State GetState(const Item& item) {
  if (IsItemInProgress(item)) {
    return IsItemPaused(item) ? ItemSortKey::kInProgressPaused
                              : ItemSortKey::kInProgressActive;
  }
  return ItemSortKey::kNotInProgress;
}

template <typename Item>
ItemSortKey GetSortKey(const Item& item) {
  return ItemSortKey{GetState(item), GetItemStartTime(item)};
}

// Helper to get an iterator to the last element in the cache. The cache
// must not be empty.
template <typename Item>
SortedItems<Item>::iterator GetLastIter(SortedItems<Item>& cache) {
  CHECK(!cache.empty());
  auto it = cache.end();
  return std::prev(it);
}

// Returns the earliest creation time for which we will show items in the
// bubble.
base::Time GetCutoffTime() {
  return base::Time::Now() - kShowItemInBubbleDuration;
}

// Updates the count of received vs total bytes. Returns whether progress is
// certain.
bool AddItemProgress(int64_t item_received_bytes,
                     int64_t item_total_bytes,
                     int& in_progress_items,
                     int64_t& received_bytes,
                     int64_t& total_bytes) {
  ++in_progress_items;
  if (item_total_bytes <= 0) {
    // Progress is uncertain: there may or may not be more data coming down this
    // pipe.
    return false;
  }
  received_bytes += item_received_bytes;
  total_bytes += item_total_bytes;
  return true;
}

bool ShouldIncludeModel(const DownloadUIModel* model, base::Time cutoff_time) {
  return DownloadUIModelIsRecent(model, cutoff_time) &&
         model->ShouldShowInBubble();
}

// Returns whether |model| was eligible to be added to |models|, regardless of
// whether it was actually added.
bool MaybeAddModel(DownloadUIModelPtr model,
                   base::Time cutoff_time,
                   size_t max_num_models,
                   std::vector<DownloadUIModelPtr>& models) {
  if (!ShouldIncludeModel(model.get(), cutoff_time)) {
    model->SetActionedOn(true);
    return false;
  }
  if (models.size() < max_num_models) {
    models.push_back(std::move(model));
  }
  return true;
}

void UpdateInfoForModel(
    const DownloadUIModel& model,
    base::Time cutoff_time,
    DownloadDisplayController::AllDownloadUIModelsInfo& info) {
  if (!ShouldIncludeModel(&model, cutoff_time)) {
    return;
  }
  ++info.all_models_size;
  info.last_completed_time =
      std::max(info.last_completed_time, model.GetEndTime());
  if (model.GetDangerType() == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
      model.GetState() != download::DownloadItem::CANCELLED) {
    info.has_deep_scanning = true;
  }
  if (!model.WasActionedOn()) {
    info.has_unactioned = true;
  }
  if (IsModelInProgress(&model)) {
    ++info.in_progress_count;
    if (model.IsPaused()) {
      ++info.paused_count;
    }
  }
}

}  // namespace

bool DownloadBubbleUpdateService::ItemSortKey::operator<(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  if (state < other.state) {
    return true;
  } else if (state > other.state) {
    return false;
  }
  return start_time > other.start_time;
}

bool DownloadBubbleUpdateService::ItemSortKey::operator==(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  return std::tie(state, start_time) == std::tie(other.state, other.start_time);
}

bool DownloadBubbleUpdateService::ItemSortKey::operator!=(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  return !(*this == other);
}

bool DownloadBubbleUpdateService::ItemSortKey::operator>(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  return !(*this == other || *this < other);
}

DownloadBubbleUpdateService::DownloadBubbleUpdateService(Profile* profile)
    : profile_(profile),
      original_profile_(profile->IsOffTheRecord()
                            ? profile_->GetOriginalProfile()
                            : nullptr) {
  offline_content_provider_observation_.Observe(
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey()));
}

DownloadBubbleUpdateService::~DownloadBubbleUpdateService() = default;

void DownloadBubbleUpdateService::Shutdown() {
  offline_content_provider_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

size_t DownloadBubbleUpdateService::GetMaxNumItemsToShow() const {
  size_t max =
      max_num_items_to_show_for_testing_.value_or(kDefaultMaxNumItemsToShow);
  CHECK_GE(max, 2u);
  return max;
}

size_t DownloadBubbleUpdateService::GetNumItemsToCache() const {
  return GetMaxNumItemsToShow() +
         extra_items_to_cache_for_testing_.value_or(kDefaultExtraItemsToCache);
}

template <typename Item>
bool DownloadBubbleUpdateService::IsCacheAtMax(const SortedItems<Item>& cache) {
  CHECK(cache.size() <= GetNumItemsToCache());
  return cache.size() == GetNumItemsToCache();
}

void DownloadBubbleUpdateService::Initialize(
    content::DownloadManager* manager) {
  CHECK(manager);
  CHECK(!download_item_notifier_);

  // Assume we have an original profile and it has an OTR profile.
  // If the original profile's DownloadBubbleUpdateService is Initialize()'d
  // already when this function is invoked on the OTR profile's
  // DownloadBubbleUpdateService, we set the OTR profile's
  // DownloadBubbleUpdateService's |original_download_item_notifier_| in the
  // 'if' block below. If the original profile's DownloadBubbleUpdateService is
  // not yet initialized when this function is invoked on the OTR profile's
  // DownloadBubbleUpdateService, we will set the OTR profile's
  // DownloadBubbleUpdateService's |original_download_item_notifier_| when the
  // original profile's DownloadBubbleUpdateService does become Initialize()'d,
  // in the 'else' block below (which will trigger re-intialization of the
  // download item cache).
  if (profile_->IsOffTheRecord()) {
    DownloadBubbleUpdateService* original_update_service =
        DownloadBubbleUpdateServiceFactory::GetForProfile(original_profile_);
    content::DownloadManager* original_download_manager =
        original_update_service ? original_update_service->GetDownloadManager()
                                : nullptr;
    if (original_download_manager) {
      InitializeOriginalNotifier(original_download_manager);
    }
  } else {
    for (Profile* otr_profile : profile_->GetAllOffTheRecordProfiles()) {
      DownloadBubbleUpdateServiceFactory::GetForProfile(otr_profile)
          ->InitializeOriginalNotifier(manager);
    }
  }
  download_item_notifier_ =
      std::make_unique<download::AllDownloadItemNotifier>(manager, this);
  // As long as we have a notifier for this profile, we can initialize the cache
  // with the current profile's downloads. If we get an original manager in the
  // future, we will initialize from scratch at that time.
  InitializeDownloadItemsCache();
  StartInitializeOfflineItemsCache();
}

void DownloadBubbleUpdateService::InitializeOriginalNotifier(
    content::DownloadManager* manager) {
  CHECK(profile_->IsOffTheRecord());
  CHECK(manager);
  if (original_download_item_notifier_) {
    return;
  }
  original_download_item_notifier_ =
      std::make_unique<download::AllDownloadItemNotifier>(manager, this);
  // Reset the download items cache, now that we have an original
  // DownloadManager to pull from.
  if (download_item_notifier_) {
    InitializeDownloadItemsCache();
  }
}

content::DownloadManager* DownloadBubbleUpdateService::GetDownloadManager() {
  return download_item_notifier_ ? download_item_notifier_->GetManager()
                                 : nullptr;
}

bool DownloadBubbleUpdateService::IsInitialized() const {
  return download_item_notifier_ && offline_items_initialized_;
}

bool DownloadBubbleUpdateService::GetAllModelsToDisplay(
    std::vector<DownloadUIModelPtr>& models,
    bool force_backfill_download_items) {
#if DCHECK_IS_ON()
  DCHECK(ConsistencyCheckCaches());
#endif  // DCHECK_IS_ON()

  base::Time cutoff_time = GetCutoffTime();
  models.clear();
  // If the caches are at max capacity, and we prune some items that are too
  // old, we may need to backfill items.
  bool download_items_cache_was_at_max = IsCacheAtMax(download_items_);
  bool offline_items_cache_was_at_max = IsCacheAtMax(offline_items_);
  bool download_item_pruned = false;
  bool offline_item_pruned = false;

  // Merge the two sorted lists, while pruning the expired items. Since the
  // criteria for pruning requires the model, to avoid unnecessary creation and
  // destruction of models, we collect the models to return and prune items in
  // the same loop iteration.
  auto download_item_it = download_items_.begin();
  auto offline_item_it = offline_items_.begin();
  while (download_item_it != download_items_.end() ||
         offline_item_it != offline_items_.end()) {
    // If the current download item sorts before the current offline item (or we
    // are out of offline items), take the download item.
    if (download_item_it != download_items_.end() &&
        (offline_item_it == offline_items_.end() ||
         download_item_it->first < offline_item_it->first)) {
      if (!MaybeAddDownloadItemModel(download_item_it->second, cutoff_time,
                                     models)) {
        download_item_it = RemoveItemFromCacheByIter(
            download_item_it, download_items_, download_items_iter_map_);
        download_item_pruned = true;
      } else {
        ++download_item_it;
      }
    } else {
      // Else, the current offline item sorts before the current download item
      // (or we are out of download items), so take the offline item.
      if (!MaybeAddOfflineItemModel(offline_item_it->second, cutoff_time,
                                    models)) {
        offline_item_it = RemoveItemFromCacheByIter(
            offline_item_it, offline_items_, offline_items_iter_map_);
        offline_item_pruned = true;
      } else {
        ++offline_item_it;
      }
    }
  }
  CHECK_LE(models.size(), GetMaxNumItemsToShow());

  bool download_items_need_backfill =
      download_item_pruned && download_items_cache_was_at_max;
  bool offline_items_need_backfill =
      offline_item_pruned && offline_items_cache_was_at_max;

  if (download_items_need_backfill) {
    // A key that will sort before any other key.
    ItemSortKey last_download_item_key{ItemSortKey::kInProgressActive,
                                       base::Time::Now()};
    if (!download_items_.empty()) {
      last_download_item_key = GetLastIter(download_items_)->first;
    }
    if (force_backfill_download_items) {
      // Get more items synchronously.
      BackfillDownloadItems(last_download_item_key);
      AppendBackfilledDownloadItems(last_download_item_key, cutoff_time,
                                    models);
      download_items_need_backfill = false;
    } else {
      StartBackfillDownloadItems(last_download_item_key);
    }
  }

  if (offline_items_need_backfill) {
    // A key that will sort before any other key.
    ItemSortKey last_offline_item_key{ItemSortKey::kInProgressActive,
                                      base::Time::Now()};
    if (!offline_items_.empty()) {
      last_offline_item_key = GetLastIter(offline_items_)->first;
    }
    StartBackfillOfflineItems(last_offline_item_key);
  }

#if DCHECK_IS_ON()
  DCHECK(ConsistencyCheckCaches());
#endif  // DCHECK_IS_ON()
  return models.size() == GetMaxNumItemsToShow() ||
         !(download_items_need_backfill || offline_items_need_backfill);
}

const DownloadDisplayController::AllDownloadUIModelsInfo&
DownloadBubbleUpdateService::GetAllModelsInfo() {
  return all_models_info_;
}

void DownloadBubbleUpdateService::UpdateAllModelsInfo() {
#if DCHECK_IS_ON()
  DCHECK(ConsistencyCheckCaches());
#endif  // DCHECK_IS_ON()

  DownloadDisplayController::AllDownloadUIModelsInfo info;
  base::Time cutoff_time = GetCutoffTime();

  // Iterate over the two sorted caches (download items and offline items) in
  // combined/merged sorted order. This is done in the same way as in
  // GetAllItemsToDisplay() to ensure that the info most accurately represents
  // the list of items that would be returned from that method.
  auto download_item_it = download_items_.begin();
  auto offline_item_it = offline_items_.begin();
  while (download_item_it != download_items_.end() ||
         offline_item_it != offline_items_.end()) {
    // If the current download item sorts before the current offline item (or we
    // are out of offline items), take the download item.
    if (download_item_it != download_items_.end() &&
        (offline_item_it == offline_items_.end() ||
         download_item_it->first < offline_item_it->first)) {
      DownloadItemModel model(download_item_it->second);
      UpdateInfoForModel(model, cutoff_time, info);
      ++download_item_it;
    } else {
      // Else, the current offline item sorts before the current download item
      // (or we are out of download items), so take the offline item.
      OfflineItemModel model(GetOfflineManager(), offline_item_it->second);
      UpdateInfoForModel(model, cutoff_time, info);
      ++offline_item_it;
    }
    if (info.all_models_size >= GetMaxNumItemsToShow()) {
      break;
    }
  }

  all_models_info_ = info;
}

ProgressInfo DownloadBubbleUpdateService::GetProgressInfo() const {
#if DCHECK_IS_ON()
  DCHECK(ConsistencyCheckCaches());
#endif  // DCHECK_IS_ON()

  ProgressInfo info;
  int in_progress_items = 0;
  int64_t received_bytes = 0;
  int64_t total_bytes = 0;

  base::Time cutoff_time = GetCutoffTime();

  for (const auto& [key, item] : download_items_) {
    if (key.state == ItemSortKey::kNotInProgress) {
      break;
    }
    if (GetItemStartTime(item) < cutoff_time) {
      continue;
    }
    // Note that operator&= does not short-circuit.
    info.progress_certain &=
        AddItemProgress(item->GetReceivedBytes(), item->GetTotalBytes(),
                        in_progress_items, received_bytes, total_bytes);
  }

  for (const auto& [key, item] : offline_items_) {
    if (key.state == ItemSortKey::kNotInProgress) {
      break;
    }
    if (GetItemStartTime(item) < cutoff_time) {
      continue;
    }
    // Note that operator&= does not short-circuit.
    info.progress_certain &=
        AddItemProgress(item.received_bytes, item.total_size_bytes,
                        in_progress_items, received_bytes, total_bytes);
  }

  info.download_count = in_progress_items;
  if (total_bytes > 0) {
    info.progress_percentage =
        base::ClampFloor(received_bytes * 100.0 / total_bytes);
  }
  return info;
}

void DownloadBubbleUpdateService::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  if (download_crx_util::IsExtensionDownload(*item) &&
      delayed_crx_guids_.size() < kMaxDelayedCrxGuids) {
    const std::string& guid = item->GetGuid();
    CHECK(!delayed_crx_guids_.contains(guid));
    delayed_crx_guids_.insert(guid);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &DownloadBubbleUpdateService::OnDelayedCrxDownloadCreated,
            weak_factory_.GetWeakPtr(), guid),
        kCrxShowNewItemDelay);
    return;
  }
  MaybeAddDownloadItemToCache(item, /*is_new=*/true);
}

void DownloadBubbleUpdateService::OnDelayedCrxDownloadCreated(
    const std::string& guid) {
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  // This assumes that for extension/theme downloads, the DownloadItem is
  // removed from the DownloadManager upon completion.
  download::DownloadItem* item =
      download_item_notifier_->GetManager()->GetDownloadByGuid(guid);
  if (item && !item->IsDone()) {
    MaybeAddDownloadItemToCache(item, /*is_new=*/true);

    Browser* browser_to_show_animation =
        FindBrowserToShowAnimation(item, profile_);
    for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
      if (browser->window() &&
          browser->window()->GetDownloadBubbleUIController()) {
        browser->window()->GetDownloadBubbleUIController()->OnDownloadItemAdded(
            item, /*may_show_animation=*/browser == browser_to_show_animation);
      }
    }
  }
  size_t erased = delayed_crx_guids_.erase(guid);
  CHECK_EQ(erased, 1u);
}

void DownloadBubbleUpdateService::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  // If the item is an extension or theme download waiting out its 2-second
  // delay, don't show a UI update for it.
  if (delayed_crx_guids_.contains(item->GetGuid())) {
    return;
  }
  bool cache_was_at_max = IsCacheAtMax(download_items_);
  bool removed_item = RemoveDownloadItemFromCache(item);
  bool added_back_at_end = MaybeAddDownloadItemToCache(item, /*is_new=*/false);
  if (cache_was_at_max && removed_item && added_back_at_end) {
    CHECK_EQ(download_items_.size(), GetNumItemsToCache());
    const ItemSortKey& last_key =
        std::prev(GetLastIter(download_items_))->first;
    StartBackfillDownloadItems(last_key);
  }

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->OnDownloadItemUpdated(
          item);
    }
  }
}

void DownloadBubbleUpdateService::OnDownloadRemoved(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  bool cache_was_at_max = IsCacheAtMax(download_items_);
  if (RemoveDownloadItemFromCache(item) && cache_was_at_max) {
    CHECK_EQ(download_items_.size(), GetNumItemsToCache() - 1);
    const ItemSortKey& last_key = GetLastIter(download_items_)->first;
    StartBackfillDownloadItems(last_key);
  }

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->OnDownloadItemRemoved(
          item);
    }
  }
}

void DownloadBubbleUpdateService::OnManagerGoingDown(
    content::DownloadManager* manager) {
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  // Assume that the original manager (if this is an OTR profile) may or may not
  // have shut down, but we still want to cease all operations when this
  // profile's manager shuts down.
  if (download_item_notifier_ &&
      (manager == download_item_notifier_->GetManager())) {
    download_items_.clear();
    download_items_iter_map_.clear();
    download_item_notifier_.reset();
  }
}

void DownloadBubbleUpdateService::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  if (offline_content_provider_shut_down_) {
    return;
  }
  if (!offline_items_initialized_) {
    offline_item_callbacks_.push_back(
        base::BindOnce(&DownloadBubbleUpdateService::OnItemsAdded,
                       weak_factory_.GetWeakPtr(), items));
    return;
  }
  for (const OfflineItem& item : items) {
    MaybeAddOfflineItemToCache(item, /*is_new=*/true);
  }

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->OnOfflineItemsAdded(
          items);
    }
  }
}

void DownloadBubbleUpdateService::OnItemRemoved(const ContentId& id) {
  if (offline_content_provider_shut_down_) {
    return;
  }
  if (!offline_items_initialized_) {
    offline_item_callbacks_.push_back(
        base::BindOnce(&DownloadBubbleUpdateService::OnItemRemoved,
                       weak_factory_.GetWeakPtr(), id));
    return;
  }
  bool cache_was_at_max = IsCacheAtMax(offline_items_);
  if (RemoveOfflineItemFromCache(id) && cache_was_at_max) {
    CHECK_EQ(offline_items_.size(), GetNumItemsToCache() - 1);
    const ItemSortKey& last_key = GetLastIter(offline_items_)->first;
    StartBackfillOfflineItems(last_key);
  }

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->OnOfflineItemRemoved(
          id);
    }
  }
}

void DownloadBubbleUpdateService::OnItemUpdated(
    const OfflineItem& item,
    const absl::optional<offline_items_collection::UpdateDelta>& update_delta) {
  if (offline_content_provider_shut_down_) {
    return;
  }
  if (!offline_items_initialized_) {
    offline_item_callbacks_.push_back(
        base::BindOnce(&DownloadBubbleUpdateService::OnItemUpdated,
                       weak_factory_.GetWeakPtr(), item, update_delta));
    return;
  }
  bool cache_was_at_max = IsCacheAtMax(offline_items_);
  bool removed_item = RemoveOfflineItemFromCache(GetItemId(item));
  bool added_back_to_end = MaybeAddOfflineItemToCache(item, /*is_new=*/false);
  if (cache_was_at_max && removed_item && added_back_to_end) {
    CHECK_EQ(offline_items_.size(), GetNumItemsToCache());
    const ItemSortKey& last_key = std::prev(GetLastIter(offline_items_))->first;
    StartBackfillOfflineItems(last_key);
  }

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->OnOfflineItemUpdated(
          item);
    }
  }
}

void DownloadBubbleUpdateService::OnContentProviderGoingDown() {
  offline_content_provider_shut_down_ = true;
  offline_items_.clear();
  offline_items_iter_map_.clear();
}

bool DownloadBubbleUpdateService::MaybeAddDownloadItemToCache(
    download::DownloadItem* item,
    bool is_new) {
  DownloadItemModel model(item);
  if (!ShouldIncludeModel(&model, GetCutoffTime())) {
    return false;
  }
  if (is_new && model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
  }
  return AddItemToCacheImpl(item, download_items_, download_items_iter_map_);
}

bool DownloadBubbleUpdateService::MaybeAddOfflineItemToCache(
    const OfflineItem& item,
    bool is_new) {
  if (profile_->IsOffTheRecord() != item.is_off_the_record) {
    return false;
  }
  if (OfflineItemUtils::IsDownload(item.id)) {
    return false;
  }
  if (item.state == offline_items_collection::OfflineItemState::CANCELLED) {
    return false;
  }
  if (item.id.name_space == ContentIndexProviderImpl::kProviderNamespace) {
    return false;
  }

  OfflineItemModel model(GetOfflineManager(), item);
  if (!ShouldIncludeModel(&model, GetCutoffTime())) {
    return false;
  }
  if (is_new && model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
  }

  return AddItemToCacheImpl(item, offline_items_, offline_items_iter_map_);
}

template <typename Id, typename Item>
bool DownloadBubbleUpdateService::AddItemToCacheImpl(
    Item item,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  // This check duplicates part of the ShouldIncludeModel() check, but is still
  // needed because we don't always call that before this function.
  if (GetItemStartTime(item) < GetCutoffTime()) {
    return false;
  }
  Id id = GetItemId(item);
  if (iter_map.contains(id)) {
    return false;
  }
  ItemSortKey key = GetSortKey(item);

  if (cache.size() >= GetNumItemsToCache()) {
    CHECK_EQ(cache.size(), GetNumItemsToCache());
    if (key > GetLastIter(cache)->first) {
      return false;
    }
  }

  auto it = cache.insert(std::make_pair(std::move(key), item));
  iter_map.insert(std::make_pair(id, it));

  while (cache.size() > GetNumItemsToCache()) {
    CHECK(!cache.empty());
    CHECK_EQ(cache.size(), 1 + GetNumItemsToCache());
    auto to_remove = GetLastIter(cache);
    const Id& id_to_remove = GetItemId(to_remove->second);
    iter_map.erase(id_to_remove);
    cache.erase(to_remove);
  }

  UpdateAllModelsInfo();

  CHECK(!cache.empty());
  auto last_it = GetLastIter(cache);
  return GetItemId(last_it->second) == id;
}

bool DownloadBubbleUpdateService::RemoveDownloadItemFromCache(
    download::DownloadItem* item) {
  return RemoveItemFromCacheImpl(GetItemId(item), download_items_,
                                 download_items_iter_map_);
}

bool DownloadBubbleUpdateService::RemoveOfflineItemFromCache(
    const ContentId& id) {
  return RemoveItemFromCacheImpl(id, offline_items_, offline_items_iter_map_);
}

template <typename Id, typename Item>
bool DownloadBubbleUpdateService::RemoveItemFromCacheImpl(
    const Id& id,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  auto iter_map_it = iter_map.find(id);
  if (iter_map_it == iter_map.end()) {
    return false;
  }

  cache.erase(iter_map_it->second);
  iter_map.erase(iter_map_it);

  UpdateAllModelsInfo();

  CHECK(cache.size() < GetNumItemsToCache());
  return true;
}

template <typename Id, typename Item>
SortedItems<Item>::iterator
DownloadBubbleUpdateService::RemoveItemFromCacheByIter(
    SortedItems<Item>::iterator iter,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  CHECK(iter != cache.end());
  auto next_iter = std::next(iter);
  iter_map.erase(GetItemId(iter->second));
  cache.erase(iter);

  UpdateAllModelsInfo();

  return next_iter;
}

void DownloadBubbleUpdateService::StartBackfillDownloadItems(
    const ItemSortKey& last_key) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadBubbleUpdateService::BackfillDownloadItems,
                     weak_factory_.GetWeakPtr(), last_key));
}

void DownloadBubbleUpdateService::BackfillDownloadItems(
    const ItemSortKey& last_key) {
  if (!download_item_notifier_) {
    return;
  }
  BackfillItemsImpl(last_key, GetAllDownloadItems(), download_items_,
                    download_items_iter_map_);
}

void DownloadBubbleUpdateService::StartBackfillOfflineItems(
    const ItemSortKey& last_key) {
  if (offline_content_provider_shut_down_) {
    return;
  }
  offline_items_collection::OfflineContentProvider* provider =
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
  provider->GetAllItems(
      base::BindOnce(&DownloadBubbleUpdateService::BackfillOfflineItems,
                     weak_factory_.GetWeakPtr(), last_key));
}

void DownloadBubbleUpdateService::BackfillOfflineItems(
    const ItemSortKey& last_key,
    const std::vector<OfflineItem>& all_items) {
  BackfillItemsImpl(last_key, all_items, offline_items_,
                    offline_items_iter_map_);
}

template <typename Id, typename Item>
void DownloadBubbleUpdateService::BackfillItemsImpl(
    const ItemSortKey& last_key,
    const std::vector<Item>& items,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  for (const Item& item : items) {
    if (GetSortKey(item) < last_key) {
      continue;
    }
    if (iter_map.contains(GetItemId(item))) {
      continue;
    }
    AddItemToCacheImpl(item, cache, iter_map);
  }
}

void DownloadBubbleUpdateService::InitializeDownloadItemsCache() {
  CHECK(download_item_notifier_);
  download_items_.clear();
  download_items_iter_map_.clear();
  for (download::DownloadItem* item : GetAllDownloadItems()) {
    MaybeAddDownloadItemToCache(item, /*is_new=*/false);
  }
}

void DownloadBubbleUpdateService::StartInitializeOfflineItemsCache() {
  if (offline_items_initialized_) {
    return;
  }
  offline_items_collection::OfflineContentProvider* provider =
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
  provider->GetAllItems(
      base::BindOnce(&DownloadBubbleUpdateService::InitializeOfflineItemsCache,
                     weak_factory_.GetWeakPtr()));
}

void DownloadBubbleUpdateService::InitializeOfflineItemsCache(
    const std::vector<OfflineItem>& all_items) {
  offline_items_.clear();
  offline_items_iter_map_.clear();
  for (const OfflineItem& item : all_items) {
    MaybeAddOfflineItemToCache(item, /*is_new=*/false);
  }
  offline_items_initialized_ = true;
  for (auto& callback : offline_item_callbacks_) {
    std::move(callback).Run();
  }
  offline_item_callbacks_.clear();
}

std::vector<download::DownloadItem*>
DownloadBubbleUpdateService::GetAllDownloadItems() {
  std::vector<download::DownloadItem*> all_items;
  if (download_item_notifier_) {
    download_item_notifier_->GetManager()->GetAllDownloads(&all_items);
  }
  if (original_download_item_notifier_) {
    original_download_item_notifier_->GetManager()->GetAllDownloads(&all_items);
  }
  return all_items;
}

OfflineItemModelManager* DownloadBubbleUpdateService::GetOfflineManager()
    const {
  return OfflineItemModelManagerFactory::GetForBrowserContext(profile_);
}

bool DownloadBubbleUpdateService::MaybeAddDownloadItemModel(
    download::DownloadItem* item,
    base::Time cutoff_time,
    std::vector<DownloadUIModelPtr>& models) {
  DownloadUIModelPtr model = DownloadItemModel::Wrap(
      item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  return MaybeAddModel(std::move(model), cutoff_time, GetMaxNumItemsToShow(),
                       models);
}

bool DownloadBubbleUpdateService::MaybeAddOfflineItemModel(
    const offline_items_collection::OfflineItem& item,
    base::Time cutoff_time,
    std::vector<DownloadUIModelPtr>& models) {
  DownloadUIModelPtr model = OfflineItemModel::Wrap(
      GetOfflineManager(), item,
      std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  return MaybeAddModel(std::move(model), cutoff_time, GetMaxNumItemsToShow(),
                       models);
}

void DownloadBubbleUpdateService::AppendBackfilledDownloadItems(
    const ItemSortKey& last_key,
    base::Time cutoff_time,
    std::vector<DownloadUIModelPtr>& models) {
  // This is not quite right because there might be a newly backfilled item
  // whose key is equal to |last_key| that this would then skip
  // over (and we would not be able to detect/fix the omission, unless the item
  // received an update later), but this should happen rarely enough (requires
  // two download items with the exact same creation time) that we will not
  // handle this case.
  auto it = download_items_.upper_bound(last_key);

  while (it != download_items_.end()) {
    if (!MaybeAddDownloadItemModel(it->second, cutoff_time, models)) {
      it = RemoveItemFromCacheByIter(it, download_items_,
                                     download_items_iter_map_);
    } else {
      ++it;
    }
  }
}

#if DCHECK_IS_ON()
bool DownloadBubbleUpdateService::ConsistencyCheckCaches() const {
  return ConsistencyCheckImpl(download_items_, download_items_iter_map_) &&
         ConsistencyCheckImpl(offline_items_, offline_items_iter_map_);
}

template <typename Id, typename Item>
bool DownloadBubbleUpdateService::ConsistencyCheckImpl(
    const SortedItems<Item>& cache,
    const IterMap<Id, Item>& iter_map) const {
  if (cache.size() != iter_map.size()) {
    DLOG(ERROR) << "Cache size " << cache.size()
                << " does not match index size " << iter_map.size() << ".";
    return false;
  }
  if (cache.size() > GetNumItemsToCache()) {
    DLOG(ERROR) << "Cache size " << cache.size() << " exceeds max size "
                << GetNumItemsToCache() << ".";
    return false;
  }
  for (auto it = cache.begin(); it != cache.end(); ++it) {
    const ItemSortKey& key = it->first;
    const Item& item = it->second;
    if (key != GetSortKey(item)) {
      DLOG(ERROR) << "Key does not match item.";
      return false;
    }
    const Id& id = GetItemId(item);
    auto iter_map_it = iter_map.find(id);
    if (iter_map_it == iter_map.end()) {
      DLOG(ERROR) << "Item id not in index.";
      return false;
    }
    if (iter_map_it->second != it) {
      DLOG(ERROR) << "Index inconsistent.";
      return false;
    }
  }
  return true;
}
#endif  // DCHECK_IS_ON()
