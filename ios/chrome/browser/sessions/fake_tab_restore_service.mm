// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/fake_tab_restore_service.h"

#import "base/run_loop.h"
#import "components/sessions/core/live_tab.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeTabRestoreService::FakeTabRestoreService() {}
FakeTabRestoreService::~FakeTabRestoreService() {}

void FakeTabRestoreService::AddObserver(
    sessions::TabRestoreServiceObserver* observer) {
  NOTREACHED();
}

void FakeTabRestoreService::RemoveObserver(
    sessions::TabRestoreServiceObserver* observer) {
  NOTREACHED();
}

absl::optional<SessionID> FakeTabRestoreService::CreateHistoricalTab(
    sessions::LiveTab* live_tab,
    int index) {
  auto tab = std::make_unique<Tab>();
  int entry_count =
      live_tab->IsInitialBlankNavigation() ? 0 : live_tab->GetEntryCount();
  tab->navigations.resize(static_cast<int>(entry_count));
  for (int i = 0; i < entry_count; ++i) {
    sessions::SerializedNavigationEntry entry = live_tab->GetEntryAtIndex(i);
    tab->navigations[i] = entry;
  }
  entries_.push_front(std::move(tab));
  return absl::nullopt;
}

void FakeTabRestoreService::BrowserClosing(sessions::LiveTabContext* context) {
  NOTREACHED();
}

void FakeTabRestoreService::BrowserClosed(sessions::LiveTabContext* context) {
  NOTREACHED();
}

void FakeTabRestoreService::CreateHistoricalGroup(
    sessions::LiveTabContext* context,
    const tab_groups::TabGroupId& group) {
  NOTREACHED();
}

void FakeTabRestoreService::GroupClosed(const tab_groups::TabGroupId& group) {
  NOTREACHED();
}

void FakeTabRestoreService::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  NOTREACHED();
}

void FakeTabRestoreService::ClearEntries() {
  NOTREACHED();
}

void FakeTabRestoreService::DeleteNavigationEntries(
    const DeletionPredicate& predicate) {
  NOTREACHED();
}

const FakeTabRestoreService::Entries& FakeTabRestoreService::entries() const {
  return entries_;
}

std::vector<sessions::LiveTab*> FakeTabRestoreService::RestoreMostRecentEntry(
    sessions::LiveTabContext* context) {
  NOTREACHED();
  return std::vector<sessions::LiveTab*>();
}

void FakeTabRestoreService::RemoveTabEntryById(SessionID session_id) {
  FakeTabRestoreService::Entries::iterator it =
      GetEntryIteratorById(session_id);
  if (it == entries_.end()) {
    return;
  }
  entries_.erase(it);
}

std::vector<sessions::LiveTab*> FakeTabRestoreService::RestoreEntryById(
    sessions::LiveTabContext* context,
    SessionID session_id,
    WindowOpenDisposition disposition) {
  NOTREACHED();
  return std::vector<sessions::LiveTab*>();
}

void FakeTabRestoreService::LoadTabsFromLastSession() {
  NOTREACHED();
}

bool FakeTabRestoreService::IsLoaded() const {
  NOTREACHED();
  return false;
}

void FakeTabRestoreService::DeleteLastSession() {
  NOTREACHED();
}

bool FakeTabRestoreService::IsRestoring() const {
  NOTREACHED();
  return false;
}

FakeTabRestoreService::Entries::iterator
FakeTabRestoreService::GetEntryIteratorById(SessionID session_id) {
  for (auto i = entries_.begin(); i != entries_.end(); ++i) {
    if ((*i)->id == session_id) {
      return i;
    }
  }
  return entries_.end();
}
