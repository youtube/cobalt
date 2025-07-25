// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_CONTROLLER_H_

#include <set>

#include "base/scoped_observation.h"
#include "chrome/browser/download/bubble/download_bubble_update_service.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/offline_item_model.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace offline_items_collection {
struct ContentId;
}

// This handles the window-level logic for controlling the download bubble.
// There is one instance of this class per browser window, and it is owned by
// the download toolbar button.
class DownloadBubbleUIController {
 public:
  // Get a valid controller for the given `download`. In the case of web apps,
  // this will always be the web app window's controller. For regular downloads,
  // this could be the controller for the most recently active window associated
  // with this profile.
  static DownloadBubbleUIController* GetForDownload(
      download::DownloadItem* download);

  explicit DownloadBubbleUIController(Browser* browser);
  // Used to inject a custom DownloadBubbleUpdateService for testing. Prefer
  // the constructor above which uses that of the profile.
  DownloadBubbleUIController(Browser* browser,
                             DownloadBubbleUpdateService* update_service);

  DownloadBubbleUIController(const DownloadBubbleUIController&) = delete;
  DownloadBubbleUIController& operator=(const DownloadBubbleUIController&) =
      delete;
  virtual ~DownloadBubbleUIController();

  // These methods are called to notify the UI of new events.
  // |may_show_animation| is whether the window this controller belongs to may
  // show the animation. (Whether the animation is actually shown may depend on
  // the download and the device's graphics capabilities.) We don't show an
  // animation for offline items. Notifications for created/added download items
  // generally come from the DownloadUIController(Delegate) (except for crx
  // downloads, which come via the DownloadBubbleUpdateService), and the rest
  // are called from DownloadBubbleUpdateService.
  void OnDownloadItemAdded(download::DownloadItem* item,
                           bool may_show_animation);
  void OnDownloadItemUpdated(download::DownloadItem* item);
  void OnDownloadItemRemoved(download::DownloadItem* item);
  void OnOfflineItemsAdded(
      const OfflineContentProvider::OfflineItemList& items);
  void OnOfflineItemUpdated(const OfflineItem& item);
  void OnOfflineItemRemoved(const ContentId& id);

  // Get the entries for the main view of the Download Bubble. The main view
  // contains all the recent downloads (finished within the last 24 hours).
  // Virtual for testing.
  virtual std::vector<DownloadUIModel::DownloadUIModelPtr> GetMainView();

  // Get the entries for the partial view of the Download Bubble. The partial
  // view contains in-progress and uninteracted downloads, meant to capture the
  // user's recent tasks. This can only be opened by the browser in the event of
  // new downloads, and user action only creates a main view.
  // Virtual for testing.
  virtual std::vector<DownloadUIModel::DownloadUIModelPtr> GetPartialView();

  // Process button press on the bubble.
  // TODO(chlily): `is_main_view` should be named `is_primary_view`. It
  // distinguishes the primary page from the (security) subpage, not the main vs
  // partial flavors of the primary view.
  void ProcessDownloadButtonPress(base::WeakPtr<DownloadUIModel> model,
                                  DownloadCommands::Command command,
                                  bool is_main_view);

  // Notify when a download toolbar button (in any window) is pressed.
  void HandleButtonPressed();

  // Opens the primary dialog to the item and scrolls to the item, and opens
  // the security dialog if the item has a security warning. Returns whether
  // bubble was opened to the requested item.
  bool OpenMostSpecificDialog(
      const offline_items_collection::ContentId& content_id);

  // Returns whether the incognito icon should be shown for the download.
  bool ShouldShowIncognitoIcon(const DownloadUIModel* model) const;

  // Returns whether the guest account icon should be shown for the download.
  bool ShouldShowGuestIcon(const DownloadUIModel* model) const;

  // Schedules the ephemeral warning download to be hidden from the bubble, and
  // subsequently canceled. It will only be canceled if it continues to be an
  // ephemeral warning that hasn't been acted on when the scheduled time
  // arrives.
  void ScheduleCancelForEphemeralWarning(const std::string& guid);

  // Force the controller to hide the download UI entirely, including the bubble
  // and the toolbar icon. This function should only be called if the event is
  // triggered outside of normal download events that are not listened by
  // observers.
  void HideDownloadUi();

  // Records that the download bubble was interacted with. This only records
  // the fact that an interaction occurred, and should not be used
  // quantitatively to count the number of such interactions.
  void RecordDownloadBubbleInteraction();

  // Returns the DownloadDisplayController. Should always return a valid
  // controller.
  DownloadDisplayController* GetDownloadDisplayController() {
    return display_controller_;
  }

  void SetDownloadDisplayController(DownloadDisplayController* controller) {
    display_controller_ = controller;
  }

  DownloadBubbleUpdateService* update_service() { return update_service_; }

  base::WeakPtr<DownloadBubbleUIController> GetWeakPtr();

 private:
  friend class DownloadBubbleUIControllerTest;
  friend class DownloadBubbleUIControllerIncognitoTest;

  // Common method for getting main and partial views.
  std::vector<DownloadUIModel::DownloadUIModelPtr> GetDownloadUIModels(
      bool is_main_view);

  // Kick off retrying an eligible interrupted download.
  void RetryDownload(DownloadUIModel* model, DownloadCommands::Command command);

  raw_ptr<Browser, DanglingUntriaged> browser_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<DownloadBubbleUpdateService, DanglingUntriaged> update_service_;
  raw_ptr<OfflineItemModelManager, DanglingUntriaged> offline_manager_;

  // DownloadDisplayController and DownloadBubbleUIController have the same
  // lifetime. Both are owned, constructed together, and destructed together by
  // DownloadToolbarButtonView. If one is valid, so is the other.
  raw_ptr<DownloadDisplayController, AcrossTasksDanglingUntriaged>
      display_controller_;

  absl::optional<base::Time> last_partial_view_shown_time_ = absl::nullopt;

  base::WeakPtrFactory<DownloadBubbleUIController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UI_CONTROLLER_H_
