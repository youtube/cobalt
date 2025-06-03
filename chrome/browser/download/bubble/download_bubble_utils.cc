// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_utils.h"

#include "base/time/time.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

base::Time GetItemStartTime(const download::DownloadItem* item) {
  return item->GetStartTime();
}

base::Time GetItemStartTime(const offline_items_collection::OfflineItem& item) {
  return item.creation_time;
}

const std::string& GetItemId(const download::DownloadItem* item) {
  return item->GetGuid();
}

const offline_items_collection::ContentId& GetItemId(
    const offline_items_collection::OfflineItem& item) {
  return item.id;
}

bool ItemIsRecent(const download::DownloadItem* item, base::Time cutoff_time) {
  return ((item->GetStartTime().is_null() && !item->IsDone()) ||
          item->GetStartTime() > cutoff_time);
}

bool ItemIsRecent(const offline_items_collection::OfflineItem& item,
                  base::Time cutoff_time) {
  // TODO(chlily): Deduplicate this code from OfflineItemModel::IsDone().
  bool is_done = false;
  switch (item.state) {
    case offline_items_collection::OfflineItemState::IN_PROGRESS:
    case offline_items_collection::OfflineItemState::PAUSED:
    case offline_items_collection::OfflineItemState::PENDING:
      break;
    case offline_items_collection::OfflineItemState::INTERRUPTED:
      is_done = item.is_resumable;
      break;
    case offline_items_collection::OfflineItemState::FAILED:
    case offline_items_collection::OfflineItemState::COMPLETE:
    case offline_items_collection::OfflineItemState::CANCELLED:
      is_done = true;
      break;
    case offline_items_collection::OfflineItemState::NUM_ENTRIES:
      NOTREACHED();
  }
  return ((item.creation_time.is_null() && !is_done) ||
          item.creation_time > cutoff_time);
}

bool DownloadUIModelIsRecent(const DownloadUIModel* model,
                             base::Time cutoff_time) {
  return ((model->GetStartTime().is_null() && !model->IsDone()) ||
          model->GetStartTime() > cutoff_time);
}

bool IsPendingDeepScanning(const download::DownloadItem* item) {
  return item->GetState() == download::DownloadItem::IN_PROGRESS &&
         item->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
}

bool IsPendingDeepScanning(const DownloadUIModel* model) {
  return model->GetState() == download::DownloadItem::IN_PROGRESS &&
         model->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
}

bool IsItemInProgress(const download::DownloadItem* item) {
  if (item->IsDangerous() || item->IsInsecure() ||
      IsPendingDeepScanning(item)) {
    return false;
  }
  return item->GetState() == download::DownloadItem::IN_PROGRESS;
}

bool IsItemInProgress(const offline_items_collection::OfflineItem& item) {
  // Offline items cannot be pending deep scanning, and insecure warnings are
  // not shown for them.
  if (item.is_dangerous) {
    return false;
  }
  return item.state ==
             offline_items_collection::OfflineItemState::IN_PROGRESS ||
         item.state == offline_items_collection::OfflineItemState::PAUSED;
}

bool IsModelInProgress(const DownloadUIModel* model) {
  if (model->IsDangerous() || model->IsInsecure() ||
      IsPendingDeepScanning(model)) {
    return false;
  }
  return model->GetState() == download::DownloadItem::IN_PROGRESS;
}

bool IsItemPaused(const download::DownloadItem* item) {
  return item->IsPaused();
}

bool IsItemPaused(const offline_items_collection::OfflineItem& item) {
  return item.state == offline_items_collection::OfflineItemState::PAUSED;
}

Browser* FindBrowserToShowAnimation(download::DownloadItem* item,
                                    Profile* profile) {
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  // For the case of DevTools web contents, we'd like to use target browser
  // shelf although saving from the DevTools web contents.
  if (web_contents && DevToolsWindow::IsDevToolsWindow(web_contents)) {
    DevToolsWindow* devtools_window =
        DevToolsWindow::AsDevToolsWindow(web_contents);
    content::WebContents* inspected =
        devtools_window->GetInspectedWebContents();
    // Do not overwrite web contents for the case of remote debugging.
    if (inspected) {
      web_contents = inspected;
    }
  }
  Browser* browser_to_show_animation =
      web_contents ? chrome::FindBrowserWithTab(web_contents) : nullptr;

  // As a last resort, use the last active browser for this profile. Not ideal,
  // but better than not showing the download at all.
  if (browser_to_show_animation == nullptr) {
    browser_to_show_animation = chrome::FindLastActiveWithProfile(profile);
  }
  return browser_to_show_animation;
}

const webapps::AppId* GetWebAppIdForBrowser(const Browser* browser) {
  return web_app::AppBrowserController::IsWebApp(browser)
             ? &browser->app_controller()->app_id()
             : nullptr;
}
