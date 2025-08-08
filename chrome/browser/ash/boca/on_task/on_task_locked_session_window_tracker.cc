// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/wm/screen_pinning_controller.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chromeos/ash/components/boca/boca_window_observer.h"
#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

// static
Browser* LockedSessionWindowTracker::GetBrowserWithTab(
    content::WebContents* tab) {
  BrowserList* const browser_list = BrowserList::GetInstance();
  for (auto browser_iterator =
           browser_list->begin_browsers_ordered_by_activation();
       browser_iterator != browser_list->end_browsers_ordered_by_activation();
       ++browser_iterator) {
    Browser* const browser = *browser_iterator;
    if (browser && browser->tab_strip_model()->GetIndexOfWebContents(tab) !=
                       TabStripModel::kNoTab) {
      return browser;
    }
  }
  return nullptr;
}

LockedSessionWindowTracker::LockedSessionWindowTracker(
    std::unique_ptr<OnTaskBlocklist> on_task_blocklist)
    : on_task_blocklist_(std::move(on_task_blocklist)),
      notifications_manager_(ash::boca::OnTaskNotificationsManager::Create()) {}

LockedSessionWindowTracker::~LockedSessionWindowTracker() {
  CleanupWindowTracker();
}

void LockedSessionWindowTracker::AddObserver(
    ash::boca::BocaWindowObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void LockedSessionWindowTracker::RemoveObserver(
    ash::boca::BocaWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LockedSessionWindowTracker::InitializeBrowserInfoForTracking(
    Browser* browser) {
  if (browser_ && browser_ != browser) {
    CleanupWindowTracker();
  }
  if (!browser || browser == browser_) {
    return;
  }
  browser_ = browser;
  browser_->tab_strip_model()->AddObserver(this);
  if (!browser_list_observation_.IsObserving()) {
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }
}

void LockedSessionWindowTracker::RefreshUrlBlocklist() {
  if (!browser_ || !browser_->tab_strip_model()->GetActiveWebContents() ||
      !browser_->tab_strip_model()
           ->GetActiveWebContents()
           ->GetLastCommittedURL()
           .is_valid()) {
    return;
  }

  on_task_blocklist_->RefreshForUrlBlocklist(
      browser_->tab_strip_model()->GetActiveWebContents());
}

void LockedSessionWindowTracker::MaybeCloseBrowser(
    base::WeakPtr<Browser> weak_browser_ptr) {
  Browser* const browser = weak_browser_ptr.get();
  // If tracking browser is in locked fullscreen mode, we may need to explicitly
  // close a browser when either a new window is opened from the OnTask SWA that
  // is blocked, but is not closed or when an OAuth is completed, but since
  // OnTask prevents windows from closing, we need to manually close that window
  // when the OAuth is completed.
  if (!browser || browser == browser_ ||
      (browser_ && !platform_util::IsBrowserLockedFullscreen(browser_)) ||
      (browser->is_type_app_popup() && oauth_in_progress_)) {
    return;
  }
  browser->window()->Close();
}

void LockedSessionWindowTracker::MaybeCloseWebContents(
    base::WeakPtr<content::WebContents> weak_tab_ptr) {
  content::WebContents* const tab = weak_tab_ptr.get();
  if (!tab || tab->GetLastCommittedURL().is_valid() ||
      on_task_blocklist()->IsParentTab(tab)) {
    return;
  }
  if (browser_->tab_strip_model()->count() > 1) {
    int index = browser_->tab_strip_model()->GetIndexOfWebContents(tab);
    if (index == TabStripModel::kNoTab) {
      return;
    }
    on_task_blocklist()->RemoveChildFilter(tab);
    browser_->tab_strip_model()->CloseWebContentsAt(index,
                                                    TabCloseTypes::CLOSE_NONE);
  }
}

void LockedSessionWindowTracker::ObserveWebContents(
    content::WebContents* web_content) {
  Observe(web_content);
}

void LockedSessionWindowTracker::set_can_start_navigation_throttle(
    bool is_ready) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  can_start_navigation_throttle_ = is_ready;
}

OnTaskBlocklist* LockedSessionWindowTracker::on_task_blocklist() {
  return on_task_blocklist_.get();
}

Browser* LockedSessionWindowTracker::browser() {
  return browser_;
}

bool LockedSessionWindowTracker::CanOpenNewPopup() {
  return can_open_new_popup_;
}

void LockedSessionWindowTracker::CleanupWindowTracker() {
  if (browser_) {
    browser_->tab_strip_model()->RemoveObserver(this);
    browser_list_observation_.Reset();
  }
  if (on_task_blocklist_) {
    on_task_blocklist_->CleanupBlocklist();
  }
  browser_ = nullptr;
  can_open_new_popup_ = true;
  oauth_in_progress_ = false;
  for (auto& observer : observers_) {
    observer.OnWindowTrackerCleanedup();
    RemoveObserver(&observer);
  }

  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(false);
  }
}

void LockedSessionWindowTracker::ShowURLBlockedToast() {
  ash::boca::OnTaskNotificationsManager::ToastCreateParams toast_create_params(
      ash::boca::kOnTaskUrlBlockedToastId,
      ash::ToastCatalogName::kOnTaskUrlBlocked,
      /*text_description_callback=*/
      base::BindRepeating([](base::TimeDelta countdown_period) {
        return l10n_util::GetStringUTF16(
            IDS_ON_TASK_URL_BLOCKED_NOTIFICATION_MESSAGE);
      }));
  notifications_manager_->CreateToast(std::move(toast_create_params));
}

// TabStripModel Implementation
void LockedSessionWindowTracker::TabChangedAt(content::WebContents* contents,
                                              int index,
                                              TabChangeType change_type) {
  if (change_type == TabChangeType::kAll) {
    RefreshUrlBlocklist();
  }

  if (browser_ && browser_->tab_strip_model()->active_index() == index) {
    // Only fire for active tab.
    for (auto& observer : observers_) {
      observer.OnActiveTabChanged(contents->GetTitle());
    }
  }
}

void LockedSessionWindowTracker::SetNotificationManagerForTesting(
    std::unique_ptr<ash::boca::OnTaskNotificationsManager>
        notifications_manager) {
  notifications_manager_ = std::move(notifications_manager);
}

void LockedSessionWindowTracker::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    RefreshUrlBlocklist();
    if (selection.new_contents) {
      for (auto& observer : observers_) {
        observer.OnActiveTabChanged(selection.new_contents->GetTitle());
      }
    }
  }
  if (change.type() == TabStripModelChange::kInserted) {
    content::WebContents* const old_contents = selection.old_contents;
    SessionID active_tab_id = SessionID::InvalidValue();
    // When new tabs are added, if the current active tab is closed or is boca
    // app homepage, then we set `active_tab_id` to be invalid.
    if (old_contents &&
        (TabStripModel::kNoTab !=
         browser_->tab_strip_model()->GetIndexOfWebContents(old_contents)) &&
        (old_contents->GetVisibleURL() !=
         GURL(ash::boca::kChromeBocaAppUntrustedIndexURL))) {
      active_tab_id = sessions::SessionTabHelper::IdForTab(old_contents);
    }
    for (const auto& contents : change.GetInsert()->contents) {
      SessionID tab_id =
          sessions::SessionTabHelper::IdForTab(contents.contents);
      GURL url = contents.contents->GetVisibleURL();
      for (auto& observer : observers_) {
        observer.OnTabAdded(active_tab_id, tab_id, url);
      }
    }
  }
}

void LockedSessionWindowTracker::OnTabWillBeRemoved(
    content::WebContents* contents,
    int index) {
  on_task_blocklist()->RemoveParentFilter(contents);
  on_task_blocklist()->RemoveChildFilter(contents);
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  for (auto& observer : observers_) {
    observer.OnTabRemoved(tab_id);
  }
}

void LockedSessionWindowTracker::WillCloseAllTabs(
    TabStripModel* tab_strip_model) {
  CHECK(tab_strip_model);

  // Force browser to skip tab unload so we can proceed with the close
  // operation.
  // TODO (crbug.com/372362860): Add browser tests to test tab unload.
  Browser* const browser = static_cast<Browser*>(
      tab_strip_model->delegate()->GetBrowserWindowInterface());
  browser->set_force_skip_warning_user_on_close(true);
}

// BrowserListObserver Implementation
void LockedSessionWindowTracker::OnBrowserClosing(Browser* browser) {
  if (browser == browser_) {
    CleanupWindowTracker();
  }
  if (browser->type() == Browser::Type::TYPE_APP_POPUP) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(false);
    can_open_new_popup_ = true;
    oauth_in_progress_ = false;
  }
}

void LockedSessionWindowTracker::OnBrowserAdded(Browser* browser) {
  if (browser->type() == Browser::Type::TYPE_APP_POPUP) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(true);
    // Since this is called after the window is created, but before we set the
    // pinning controller to allow the popup window to be on top of the
    // pinned window, we need to explicitly move this `browser` to be on top.
    // Otherwise, the popup window would still be beneath the pinned window.
    aura::Window* const top_container =
        ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                 ash::kShellWindowId_AlwaysOnTopContainer);
    top_container->StackChildAtTop(browser->window()->GetNativeWindow());
    can_open_new_popup_ = false;
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseBrowser,
                       weak_pointer_factory_.GetWeakPtr(),
                       browser->AsWeakPtr()));
  }
}

// content::WebContentsObserver Impl
void LockedSessionWindowTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  Browser* const browser =
      GetBrowserWithTab(navigation_handle->GetWebContents());
  if (!browser || !browser_) {
    return;
  }
  if (browser != browser_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseBrowser,
                       weak_pointer_factory_.GetWeakPtr(),
                       browser->AsWeakPtr()));
  } else {
    content::WebContents* const tab = navigation_handle->GetWebContents();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseWebContents,
                       weak_pointer_factory_.GetWeakPtr(), tab->GetWeakPtr()));
  }
}
