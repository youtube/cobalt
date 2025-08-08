// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/web_contents_state_synced_tab_delegate.h"

namespace browser_sync {

WebContentsStateSyncedTabDelegate::WebContentsStateSyncedTabDelegate(
    TabAndroid* tab_android,
    std::unique_ptr<WebContentsStateByteBuffer> web_contents_byte_buffer)
    : tab_android_(tab_android),
      web_contents_buffer_(std::move(web_contents_byte_buffer)) {
  web_contents_ = WebContentsState::RestoreContentsFromByteBuffer(
      web_contents_buffer_.get(), /*initially_hidden=*/true,
      /*no_renderer=*/true);
  if (web_contents_) {
    web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  }
  TabContentsSyncedTabDelegate::SetWebContents(web_contents_.get());
}

WebContentsStateSyncedTabDelegate::~WebContentsStateSyncedTabDelegate() =
    default;

std::unique_ptr<WebContentsStateSyncedTabDelegate>
WebContentsStateSyncedTabDelegate::Create(
    TabAndroid* tab_android,
    std::unique_ptr<WebContentsStateByteBuffer> web_contents_byte_buffer) {
  auto tab_delegate = base::WrapUnique(new WebContentsStateSyncedTabDelegate(
      tab_android, std::move(web_contents_byte_buffer)));

  // If the retrieved web contents of the newly created delegate is still null,
  // indicate for an early exit in the AssociatePlaceholderTab snapshot catch.
  if (!tab_delegate->HasWebContents()) {
    return nullptr;
  }
  return tab_delegate;
}

SessionID WebContentsStateSyncedTabDelegate::GetWindowId() const {
  return tab_android_->window_id();
}

SessionID WebContentsStateSyncedTabDelegate::GetSessionId() const {
  return browser_sync::SyncedTabDelegateAndroid::SessionIdFromAndroidId(
      tab_android_->GetAndroidId());
}

bool WebContentsStateSyncedTabDelegate::IsPlaceholderTab() const {
  // This will always return false, as this tab delegate is created as an
  // attempt to resync a placeholder tab. That tab should no longer be a
  // placeholder tab after creation. If the web contents of the newly
  // created delegate are still null, the creation will return a nullptr
  // which will be caught in checks downstream at
  // local_session_event_handler.cc.
  CHECK(web_contents());
  return false;
}

bool WebContentsStateSyncedTabDelegate::HasWebContents() const {
  return web_contents() != nullptr;
}

std::unique_ptr<sync_sessions::SyncedTabDelegate>
WebContentsStateSyncedTabDelegate::CreatePlaceholderTabSyncedTabDelegate() {
  NOTREACHED() << "CreatePlaceholderTabSyncedTabDelegate should be called only "
                  "via the SyncedTabDelegateAndroid implementation.";
  return nullptr;
}

}  // namespace browser_sync
