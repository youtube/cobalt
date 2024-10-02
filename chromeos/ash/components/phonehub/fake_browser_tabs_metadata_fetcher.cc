// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_browser_tabs_metadata_fetcher.h"

namespace ash {

namespace phonehub {

FakeBrowserTabsMetadataFetcher::FakeBrowserTabsMetadataFetcher() = default;

FakeBrowserTabsMetadataFetcher::~FakeBrowserTabsMetadataFetcher() = default;

void FakeBrowserTabsMetadataFetcher::Fetch(
    const sync_sessions::SyncedSession* session,
    base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) {
  session_ = session;
  callback_ = std::move(callback);
}

void FakeBrowserTabsMetadataFetcher::FetchForeignSyncedPhoneSessionMetadata(
    const ash::ForeignSyncedSessionAsh& session,
    SyncedSessionClientAsh* synced_session_client_ash,
    base::OnceCallback<void(BrowserTabsMetadataResponse)> callback) {
  foreign_synced_session_ = &session;
  callback_ = std::move(callback);
}

void FakeBrowserTabsMetadataFetcher::RespondToCurrentFetchAttempt(
    const BrowserTabsMetadataResponse& response) {
  std::move(callback_).Run(response);
}

bool FakeBrowserTabsMetadataFetcher::DoesPendingCallbackExist() {
  return !callback_.is_null();
}

const sync_sessions::SyncedSession* FakeBrowserTabsMetadataFetcher::GetSession()
    const {
  return session_;
}

const ash::ForeignSyncedSessionAsh*
FakeBrowserTabsMetadataFetcher::GetForeignSyncedSession() const {
  return foreign_synced_session_;
}

}  // namespace phonehub
}  // namespace ash
