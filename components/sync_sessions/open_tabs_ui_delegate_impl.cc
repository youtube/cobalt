// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"

namespace sync_sessions {

OpenTabsUIDelegateImpl::OpenTabsUIDelegateImpl(
    const SyncSessionsClient* sessions_client,
    const SyncedSessionTracker* session_tracker,
    const DeleteForeignSessionCallback& delete_foreign_session_cb)
    : sessions_client_(sessions_client),
      session_tracker_(session_tracker),
      delete_foreign_session_cb_(delete_foreign_session_cb) {}

OpenTabsUIDelegateImpl::~OpenTabsUIDelegateImpl() = default;

bool OpenTabsUIDelegateImpl::GetAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) {
  *sessions = session_tracker_->LookupAllForeignSessions(
      SyncedSessionTracker::PRESENTABLE);
  base::ranges::sort(
      *sessions, std::greater(),
      [](const SyncedSession* session) { return session->GetModifiedTime(); });
  return !sessions->empty();
}

bool OpenTabsUIDelegateImpl::GetForeignSession(
    const std::string& tag,
    std::vector<const sessions::SessionWindow*>* windows) {
  return session_tracker_->LookupSessionWindows(tag, windows);
}

bool OpenTabsUIDelegateImpl::GetForeignTab(const std::string& tag,
                                           const SessionID tab_id,
                                           const sessions::SessionTab** tab) {
  *tab = session_tracker_->LookupSessionTab(tag, tab_id);
  return *tab != nullptr;
}

bool OpenTabsUIDelegateImpl::GetForeignSessionTabs(
    const std::string& tag,
    std::vector<const sessions::SessionTab*>* tabs) {
  std::vector<const sessions::SessionWindow*> windows;
  if (!session_tracker_->LookupSessionWindows(tag, &windows)) {
    return false;
  }

  // Prune those tabs that are not syncable or are NewTabPage, then sort them
  // from most recent to least recent, independent of which window the tabs were
  // from.
  for (const sessions::SessionWindow* window : windows) {
    for (const std::unique_ptr<sessions::SessionTab>& tab : window->tabs) {
      if (tab->navigations.empty()) {
        continue;
      }
      const sessions::SerializedNavigationEntry& current_navigation =
          tab->navigations.at(tab->normalized_navigation_index());
      if (!sessions_client_->ShouldSyncURL(current_navigation.virtual_url())) {
        continue;
      }
      tabs->push_back(tab.get());
    }
  }
  base::ranges::stable_sort(
      *tabs, std::greater(),
      [](const sessions::SessionTab* tab) { return tab->timestamp; });
  return true;
}

void OpenTabsUIDelegateImpl::DeleteForeignSession(const std::string& tag) {
  delete_foreign_session_cb_.Run(tag);
}

bool OpenTabsUIDelegateImpl::GetLocalSession(
    const SyncedSession** local_session) {
  *local_session = session_tracker_->LookupLocalSession();
  return *local_session != nullptr;
}

}  // namespace sync_sessions
