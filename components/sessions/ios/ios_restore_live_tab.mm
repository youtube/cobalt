// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_restore_live_tab.h"

#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#include "ios/web/public/session/crw_navigation_item_storage.h"
#include "ios/web/public/session/crw_session_storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sessions {

RestoreIOSLiveTab::RestoreIOSLiveTab(CRWSessionStorage* session)
    : session_(session) {}

RestoreIOSLiveTab::~RestoreIOSLiveTab() {}

bool RestoreIOSLiveTab::IsInitialBlankNavigation() {
  return false;
}

int RestoreIOSLiveTab::GetCurrentEntryIndex() {
  return session_.lastCommittedItemIndex;
}

int RestoreIOSLiveTab::GetPendingEntryIndex() {
  return -1;
}

sessions::SerializedNavigationEntry RestoreIOSLiveTab::GetEntryAtIndex(
    int index) {
  NSArray<CRWNavigationItemStorage*>* item_storages = session_.itemStorages;
  CRWNavigationItemStorage* item = item_storages[index];
  return sessions::IOSSerializedNavigationBuilder::FromNavigationStorageItem(
      index, item);
}

sessions::SerializedNavigationEntry RestoreIOSLiveTab::GetPendingEntry() {
  return sessions::SerializedNavigationEntry();
}

int RestoreIOSLiveTab::GetEntryCount() {
  return session_.itemStorages.count;
}

sessions::SerializedUserAgentOverride
RestoreIOSLiveTab::GetUserAgentOverride() {
  return sessions::SerializedUserAgentOverride();
}

const web::WebState* RestoreIOSLiveTab::GetWebState() const {
  return nullptr;
}

}  // namespace sessions
