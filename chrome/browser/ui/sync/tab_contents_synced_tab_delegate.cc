// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "components/translate/content/browser/content_record_page_language.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

using content::NavigationEntry;

namespace {

// Helper to access the correct NavigationEntry, accounting for pending entries.
NavigationEntry* GetPossiblyPendingEntryAtIndex(
    content::WebContents* web_contents,
    int i) {
  int pending_index = web_contents->GetController().GetPendingEntryIndex();
  if (pending_index == i) {
    return web_contents->GetController().GetPendingEntry();
  }
  NavigationEntry* entry = web_contents->GetController().GetEntryAtIndex(i);
  // Don't use the entry for sync if it doesn't exist or is the initial
  // NavigationEntry.
  // TODO(https://crbug.com/1240138): Guarantee this won't be called when on the
  // initial NavigationEntry instead of bailing out here.
  if (!entry || entry->IsInitialEntry()) {
    return nullptr;
  }
  return entry;
}

}  // namespace

TabContentsSyncedTabDelegate::TabContentsSyncedTabDelegate()
    : web_contents_(nullptr) {}

TabContentsSyncedTabDelegate::~TabContentsSyncedTabDelegate() = default;

bool TabContentsSyncedTabDelegate::IsBeingDestroyed() const {
  return web_contents_->IsBeingDestroyed();
}

std::string TabContentsSyncedTabDelegate::GetExtensionAppId() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return apps::GetAppIdForWebContents(web_contents_);
#else
  return std::string();
#endif
}

bool TabContentsSyncedTabDelegate::IsInitialBlankNavigation() const {
  return web_contents_->GetController().IsInitialBlankNavigation();
}

int TabContentsSyncedTabDelegate::GetCurrentEntryIndex() const {
  return web_contents_->GetController().GetCurrentEntryIndex();
}

int TabContentsSyncedTabDelegate::GetEntryCount() const {
  return web_contents_->GetController().GetEntryCount();
}

GURL TabContentsSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  return entry ? entry->GetVirtualURL() : GURL();
}

std::string TabContentsSyncedTabDelegate::GetPageLanguageAtIndex(int i) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  // If we don't have an entry, return empty language.
  return entry ? translate::GetPageLanguageFromNavigation(entry)
               : std::string();
}

void TabContentsSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  if (entry) {
    // Explicitly exclude page state when serializing the navigation entry.
    // Sync ignores the page state anyway (e.g. form data is not synced), and
    // the page state can be expensive to serialize.
    *serialized_entry =
        sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
            i, entry,
            sessions::ContentSerializedNavigationBuilder::EXCLUDE_PAGE_STATE);
  }
}

bool TabContentsSyncedTabDelegate::ProfileHasChildAccount() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->IsChild();
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
TabContentsSyncedTabDelegate::GetBlockedNavigations() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents_);
  DCHECK(navigation_observer);

  return &navigation_observer->blocked_navigations();
#else
  NOTREACHED();
  return nullptr;
#endif
}

bool TabContentsSyncedTabDelegate::ShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  if (sessions_client->GetSyncedWindowDelegatesGetter()->FindById(
          GetWindowId()) == nullptr) {
    return false;
  }

  if (ProfileHasChildAccount() && !GetBlockedNavigations()->empty()) {
    return true;
  }

  if (IsInitialBlankNavigation()) {
    return false;  // This deliberately ignores a new pending entry.
  }

  // Don't try to sync the initial NavigationEntry, as it is not actually
  // associated with any navigation.
  content::NavigationEntry* last_committed_entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (last_committed_entry->IsInitialEntry()) {
    return false;
  }

  int entry_count = GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    const GURL& virtual_url = GetVirtualURLAtIndex(i);
    if (!virtual_url.is_valid()) {
      continue;
    }

    if (sessions_client->ShouldSyncURL(virtual_url)) {
      return true;
    }
  }
  return false;
}

int64_t TabContentsSyncedTabDelegate::GetTaskIdForNavigationId(
    int nav_id) const {
  const tasks::TaskTabHelper* task_tab_helper = this->task_tab_helper();
  if (task_tab_helper &&
      task_tab_helper->get_task_id_for_navigation(nav_id) != nullptr) {
    return task_tab_helper->get_task_id_for_navigation(nav_id)->id();
  }
  return -1;
}

int64_t TabContentsSyncedTabDelegate::GetParentTaskIdForNavigationId(
    int nav_id) const {
  const tasks::TaskTabHelper* task_tab_helper = this->task_tab_helper();
  if (task_tab_helper &&
      task_tab_helper->get_task_id_for_navigation(nav_id) != nullptr) {
    return task_tab_helper->get_task_id_for_navigation(nav_id)->parent_id();
  }
  return -1;
}

int64_t TabContentsSyncedTabDelegate::GetRootTaskIdForNavigationId(
    int nav_id) const {
  const tasks::TaskTabHelper* task_tab_helper = this->task_tab_helper();
  if (task_tab_helper &&
      task_tab_helper->get_task_id_for_navigation(nav_id) != nullptr) {
    return task_tab_helper->get_task_id_for_navigation(nav_id)->root_id();
  }
  return -1;
}

const content::WebContents* TabContentsSyncedTabDelegate::web_contents() const {
  return web_contents_;
}

content::WebContents* TabContentsSyncedTabDelegate::web_contents() {
  return web_contents_;
}

void TabContentsSyncedTabDelegate::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
}

const tasks::TaskTabHelper* TabContentsSyncedTabDelegate::task_tab_helper()
    const {
  if (web_contents_ == nullptr) {
    return nullptr;
  }
  return tasks::TaskTabHelper::FromWebContents(web_contents_);
}
