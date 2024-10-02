// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_H_

#include <map>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class DownloadManager;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

// Caches download items and offline items in sorted order, so that UI updates
// can be processed more quickly without fetching and sorting all items every
// time. Passes notifications on to the window-level UI controllers.
class DownloadBubbleUpdateService
    : public KeyedService,
      public download::AllDownloadItemNotifier::Observer,
      public offline_items_collection::OfflineContentProvider::Observer {
 public:
  // Defines sort priority for items.
  struct ItemSortKey {
    enum State {
      kInProgressActive = 0,
      kInProgressPaused = 1,
      kNotInProgress = 2,
    };

    bool operator<(const ItemSortKey& other) const;
    bool operator==(const ItemSortKey& other) const;
    bool operator!=(const ItemSortKey& other) const;
    bool operator>(const ItemSortKey& other) const;

    // Active in-progress items come before paused items, which come before
    // not-in-progress items.
    State state;
    // Within each state, items are sorted in reverse chronological order by
    // start time (most recent first).
    base::Time start_time;
  };

  template <typename Item>
  using SortedItems = std::multimap<ItemSortKey, Item>;
  template <typename Id, typename Item>
  using IterMap = std::map<Id, typename SortedItems<Item>::iterator>;

  using SortedDownloadItems = SortedItems<download::DownloadItem*>;
  using SortedOfflineItems = SortedItems<offline_items_collection::OfflineItem>;
  using DownloadItemIterMap = IterMap<std::string, download::DownloadItem*>;
  using OfflineItemIterMap = IterMap<offline_items_collection::ContentId,
                                     offline_items_collection::OfflineItem>;

  explicit DownloadBubbleUpdateService(Profile* profile);
  DownloadBubbleUpdateService(const DownloadBubbleUpdateService&) = delete;
  DownloadBubbleUpdateService& operator=(const DownloadBubbleUpdateService&) =
      delete;

  ~DownloadBubbleUpdateService() override;

  // Gets models for the top GetMaxNumItemsToShow() combined download items
  // and offline items, in sorted order. May cause items to be pruned from the
  // cache, if they have grown too old to be included. May trigger backfilling
  // the caches, but does not wait for backfill results, unless
  // |force_backfill_download_items| is true (in which case download items will
  // be backfilled synchronously if necessary; offline items will not be
  // backfilled synchronously). |models| is cleared. Returns whether results are
  // complete. Results may not be complete if there might be more items to be
  // returned after backfilling. Virtual for testing.
  virtual bool GetAllModelsToDisplay(
      std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
      bool force_backfill_download_items = false);

  // Returns information relevant to the display state of the download button.
  // Does not prune the cache or backfill missing items. May be slightly
  // inaccurate in edge cases. Virtual for testing.
  virtual const DownloadDisplayController::AllDownloadUIModelsInfo&
  GetAllModelsInfo();

  // Computes progress info based on in-progress downloads. Does not prune the
  // cache or backfill missing items, so the returned progress info may be
  // slightly inaccurate in edge cases. This is ok, as it is only for the
  // purpose of showing a progress ring around the icon, which is not precise
  // anyway. Virtual for testing.
  virtual DownloadDisplayController::ProgressInfo GetProgressInfo() const;

  // Initializes AllDownloadItemNotifier for the current profile, and
  // initializes caches. This is called when the manager is ready, to signal
  // that the DownloadBubbleUpdateService should begin tracking downloads. This
  // starts initialization of both the download items and the offline items.
  // Should only be called once.
  void Initialize(content::DownloadManager* manager);

  // Initializes the AllDownloadItemNotifier for the original profile, if
  // |profile_| is off the record. May trigger re-initialization of the download
  // items cache.
  void InitializeOriginalNotifier(content::DownloadManager* manager);

  // Get the DownloadManager that |download_item_notifier_| is listening to.
  content::DownloadManager* GetDownloadManager();

  // Virtual for testing.
  virtual bool IsInitialized() const;

  // KeyedService:
  void Shutdown() override;

  // download::AllDownloadItemNotifier::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  // offline_items_collection::OfflineContentProvider::Observer:
  void OnItemsAdded(
      const offline_items_collection::OfflineContentProvider::OfflineItemList&
          items) override;
  void OnItemRemoved(const offline_items_collection::ContentId& id) override;
  void OnItemUpdated(
      const offline_items_collection::OfflineItem& item,
      const absl::optional<offline_items_collection::UpdateDelta>& update_delta)
      override;
  void OnContentProviderGoingDown() override;

  // Logic in this class assumes the max is at least 2.
  void set_max_num_items_to_show_for_testing(size_t max) {
    max_num_items_to_show_for_testing_ = max;
  }

  void set_extra_items_to_cache_for_testing(size_t items) {
    extra_items_to_cache_for_testing_ = items;
  }

  download::AllDownloadItemNotifier& download_item_notifier_for_testing() {
    return *download_item_notifier_;
  }

  download::AllDownloadItemNotifier&
  original_download_item_notifier_for_testing() {
    return *original_download_item_notifier_;
  }

 private:
  // Returns the max number of combined download items and offline items that
  // will be returned from GetAllModelsToDisplay().
  size_t GetMaxNumItemsToShow() const;

  // Returns the max number of items (of each type) to cache. This is slightly
  // more than the max number of items to show.
  size_t GetNumItemsToCache() const;

  // Whether the cache is at its max allowed capacity.
  template <typename Item>
  bool IsCacheAtMax(const SortedItems<Item>& cache);

  // Adds an item to the cache if it is recent enough and meets other criteria
  // for showing in the bubble. If adding an item makes the map size exceed the
  // maximum, this removes excess items from the end of the map. Returns whether
  // the item was stored as the last item in the map. If |item| was already in
  // the cache, this does nothing. |is_new| is whether the item is a newly added
  // item (as opposed to an updated one). May mark the item model as
  // not-actioned-on if the item is new.
  bool MaybeAddDownloadItemToCache(download::DownloadItem* item, bool is_new);
  bool MaybeAddOfflineItemToCache(
      const offline_items_collection::OfflineItem& item,
      bool is_new);

  template <typename Id, typename Item>
  bool AddItemToCacheImpl(Item item,
                          SortedItems<Item>& cache,
                          IterMap<Id, Item>& iter_map);

  // Removes an item from the maps. Note: If the cache size was already at the
  // limit, and removing an item brings it under that limit, we must then get
  // all items in order to backfill the newly created space. (See
  // Backfill*Items() below.) Returns whether item was removed. If |item| is not
  // already in the cache, this does nothing.
  bool RemoveDownloadItemFromCache(download::DownloadItem* item);
  bool RemoveOfflineItemFromCache(
      const offline_items_collection::ContentId& id);

  template <typename Id, typename Item>
  bool RemoveItemFromCacheImpl(const Id& id,
                               SortedItems<Item>& cache,
                               IterMap<Id, Item>& iter_map);

  // Removes item if we already have the iterator to it. Returns next iterator.
  template <typename Id, typename Item>
  SortedItems<Item>::iterator RemoveItemFromCacheByIter(
      SortedItems<Item>::iterator iter,
      SortedItems<Item>& cache,
      IterMap<Id, Item>& iter_map);

  // Gets all items from the DownloadManager/ContentProvider, finds the top
  // items that sort at or after |last_key| and adds them to the cache such that
  // the total number of items does not exceed the max. The Start*() versions
  // just post a task to kick off backfilling while the other two perform the
  // backfilling synchronously. Note that it is ok if other additions/deletions
  // happen while the backfill task is queued. If an item is inserted before
  // last_key then it would have been there anyway. If an item is inserted
  // after last_key, it is the same as if it were added during backfilling. If
  // an item is removed before last_key, then there is just more space to
  // backfill.
  void StartBackfillDownloadItems(const ItemSortKey& last_key);
  void BackfillDownloadItems(const ItemSortKey& last_key);
  void StartBackfillOfflineItems(const ItemSortKey& last_key);
  void BackfillOfflineItems(
      const ItemSortKey& last_key,
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  template <typename Id, typename Item>
  void BackfillItemsImpl(const ItemSortKey& last_key,
                         const std::vector<Item>& items,
                         SortedItems<Item>& cache,
                         IterMap<Id, Item>& iter_map);

  // Populate the cache from items fetched from the download manager or
  // offline content manager.
  void InitializeDownloadItemsCache();
  void StartInitializeOfflineItemsCache();
  void InitializeOfflineItemsCache(
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  // Gets download items from profile and original profile.
  std::vector<download::DownloadItem*> GetAllDownloadItems();

  OfflineItemModelManager* GetOfflineManager() const;

  // Wraps an item into a DownloadUIModel and possibly adds it to |models|, if
  // it is new enough (newer than |cutoff_time|) and meets other criteria.
  // Returns whether model was eligible to be added, regardless of whether it
  // was added (it may not have been added if |models| was at the size limit).
  bool MaybeAddDownloadItemModel(
      download::DownloadItem* item,
      base::Time cutoff_time,
      std::vector<DownloadUIModel::DownloadUIModelPtr>& models);
  bool MaybeAddOfflineItemModel(
      const offline_items_collection::OfflineItem& item,
      base::Time cutoff_time,
      std::vector<DownloadUIModel::DownloadUIModelPtr>& models);

  // Append newly backfilled download items to |models|. |last_key| is the last
  // key that was processed before backfilling. May prune any expired items
  // (i.e. items older than |cutoff_time|).
  void AppendBackfilledDownloadItems(
      const ItemSortKey& last_key,
      base::Time cutoff_time,
      std::vector<DownloadUIModel::DownloadUIModelPtr>& models);

  // Called when a crx download has waited out its 2 second delay. Adds the
  // item to the cache if it's not already done, and notifies window-level
  // controllers.
  void OnDelayedCrxDownloadCreated(const std::string& guid);

  // Updates |all_models_info_| based on the current contents of the cache.
  // This is kept updated as items are added or removed from the cache.
  void UpdateAllModelsInfo();

#if DCHECK_IS_ON()
  // Checks that the cache data structures are consistent.
  bool ConsistencyCheckCaches() const;

  template <typename Id, typename Item>
  bool ConsistencyCheckImpl(const SortedItems<Item>& cache,
                            const IterMap<Id, Item>& iter_map) const;
#endif  // DCHECK_IS_ON()

  // Profile corresponding to this object.
  const raw_ptr<Profile> profile_ = nullptr;
  // Null if the profile is not OTR.
  const raw_ptr<Profile> original_profile_ = nullptr;

  // Override for the number of combined items to return.
  absl::optional<size_t> max_num_items_to_show_for_testing_;
  // Override for the number of extra items to cache.
  absl::optional<size_t> extra_items_to_cache_for_testing_;

  // Caches the current most-relevant items in sorted order. Size of each map
  // will generally be limited to GetMaxNumItemsToShow (except during addition
  // of an item). Note: These are multimaps because, in theory, multiple items
  // might have the same sort key. The cache manipulation logic in this class
  // accounts for this by assuming that, if there's not enough space for all the
  // items with the last key, then caching an arbitrary subset of them is fine.
  SortedDownloadItems download_items_;
  SortedOfflineItems offline_items_;

  // Holds iterators pointing into the above two maps, allowing lookup of an
  // item by GUID or ContentId.
  DownloadItemIterMap download_items_iter_map_;
  OfflineItemIterMap offline_items_iter_map_;

  // Notifier for the current profile's DownloadManager. Null until initialized
  // in Initialize().
  std::unique_ptr<download::AllDownloadItemNotifier> download_item_notifier_;
  // Null if the profile is not OTR. Null until the original profile initiates
  // a download. If the profile is OTR, this holds a notifier for the original
  // profile.
  std::unique_ptr<download::AllDownloadItemNotifier>
      original_download_item_notifier_;

  bool offline_items_initialized_ = false;
  // Holds functions queued up while offline items were being initialized.
  std::vector<base::OnceClosure> offline_item_callbacks_;

  bool offline_content_provider_shut_down_ = false;

  // Set of GUIDs for extension/theme (crx) downloads that are pending notifying
  // the UI. GUIDs are added here when the download begins, and are removed
  // when the 2 second delay is up.
  std::set<std::string> delayed_crx_guids_;

  // Holds the latest info about all models, relevant to the display state of
  // the download toolbar icon.
  DownloadDisplayController::AllDownloadUIModelsInfo all_models_info_;

  // Observes the offline content provider.
  base::ScopedObservation<
      offline_items_collection::OfflineContentProvider,
      offline_items_collection::OfflineContentProvider::Observer>
      offline_content_provider_observation_{this};

  base::WeakPtrFactory<DownloadBubbleUpdateService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_H_
