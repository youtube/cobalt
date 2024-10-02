// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

namespace content {
class WebContents;
}

class TabAndroid;

namespace browser_sync {
// On Android a tab can exist even without web contents.

// SyncedTabDelegateAndroid specializes TabContentsSyncedTabDelegate with
// support for setting web contents on a late stage (for placeholder tabs),
// when the tab is brought to memory.
class SyncedTabDelegateAndroid : public TabContentsSyncedTabDelegate {
 public:
  explicit SyncedTabDelegateAndroid(TabAndroid* owning_tab_);

  SyncedTabDelegateAndroid(const SyncedTabDelegateAndroid&) = delete;
  SyncedTabDelegateAndroid& operator=(const SyncedTabDelegateAndroid&) = delete;

  ~SyncedTabDelegateAndroid() override;

  // SyncedTabDelegate:
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsPlaceholderTab() const override;

  // Set the web contents for this tab.
  void SetWebContents(content::WebContents* web_contents);

  // Set web contents to null.
  void ResetWebContents();

 private:
  raw_ptr<TabAndroid> tab_android_;
};
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_TAB_DELEGATE_ANDROID_H_
