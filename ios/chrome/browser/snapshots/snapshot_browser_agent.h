// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

@class SnapshotCache;

// Associates a SnapshotCache to a Browser.
class SnapshotBrowserAgent : public BrowserObserver,
                             public WebStateListObserver,
                             public BrowserUserData<SnapshotBrowserAgent> {
 public:
  SnapshotBrowserAgent(const SnapshotBrowserAgent&) = delete;
  SnapshotBrowserAgent& operator=(const SnapshotBrowserAgent&) = delete;

  ~SnapshotBrowserAgent() override;

  // Set a session identification string that will be used to locate the
  // snapshots directory. Setting this more than once on the same agent is
  // probably a programming error.
  void SetSessionID(NSString* session_identifier);

  // Maintains the snapshots storage including purging unused images and
  // performing any necessary migrations.
  void PerformStorageMaintenance();

  // Permanently removes all snapshots.
  void RemoveAllSnapshots();

  SnapshotCache* snapshot_cache() { return snapshot_cache_; }

 private:
  friend class BrowserUserData<SnapshotBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit SnapshotBrowserAgent(Browser* browser);

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

  // Migrates the snapshot storage if a folder exists in the old snapshots
  // storage location.
  void MigrateStorageIfNecessary();

  // Purges the snapshots folder of unused snapshots.
  void PurgeUnusedSnapshots();

  // Returns the Tab IDs of all the WebStates in the Browser.
  NSSet<NSString*>* GetTabIDs();

  __strong SnapshotCache* snapshot_cache_;

  Browser* browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_
