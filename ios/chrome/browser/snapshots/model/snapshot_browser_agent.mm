// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/ios/ios_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"

namespace {

// Name of the directory containing the tab snapshots.
const base::FilePath::CharType kSnapshots[] = FILE_PATH_LITERAL("Snapshots");

}  // anonymous namespace

BROWSER_USER_DATA_KEY_IMPL(SnapshotBrowserAgent)

SnapshotBrowserAgent::SnapshotBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
}

SnapshotBrowserAgent::~SnapshotBrowserAgent() = default;

#pragma mark - BrowserObserver

void SnapshotBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  browser_ = nullptr;
  [snapshot_storage_ shutdown];
}

#pragma mark - WebStateListObserver

void SnapshotBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      DetachWebState(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      DetachWebState(replace_change.replaced_web_state());
      InsertWebState(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      InsertWebState(insert_change.inserted_web_state());
      break;
    }
  }
}

void SnapshotBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); ++i) {
    DetachWebState(web_state_list->GetWebStateAt(i));
  }
}

void SnapshotBrowserAgent::BatchOperationEnded(WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); ++i) {
    InsertWebState(web_state_list->GetWebStateAt(i));
  }
}

void SnapshotBrowserAgent::SetSessionID(NSString* session_identifier) {
  // It is incorrect to call this method twice.
  DCHECK(!snapshot_storage_);
  DCHECK(session_identifier.length != 0);

  const std::string identifier = base::SysNSStringToUTF8(session_identifier);

  const base::FilePath& browser_state_path =
      browser_->GetBrowserState()->GetStatePath();

  // The snapshots are stored in a sub-directory of the session storage.
  // TODO(crbug.com/1383087): change this before launching the optimised
  // session storage as the session directory will be renamed.
  const base::FilePath legacy_path =
      browser_state_path.Append(FILE_PATH_LITERAL("Sessions"))
          .Append(identifier)
          .Append(kSnapshots);

  const base::FilePath storage_path =
      browser_state_path.Append(kSnapshots).Append(identifier);

  snapshot_storage_ = [[SnapshotStorage alloc] initWithStoragePath:storage_path
                                                        legacyPath:legacy_path];
}

void SnapshotBrowserAgent::PerformStorageMaintenance() {
  MigrateStorageIfNecessary();
  PurgeUnusedSnapshots();
}

void SnapshotBrowserAgent::RemoveAllSnapshots() {
  [snapshot_storage_ removeAllImages];
}

void SnapshotBrowserAgent::InsertWebState(web::WebState* web_state) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotStorage(
      snapshot_storage_);
}

void SnapshotBrowserAgent::DetachWebState(web::WebState* web_state) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotStorage(nil);
}

void SnapshotBrowserAgent::MigrateStorageIfNecessary() {
  DCHECK(snapshot_storage_);

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int web_state_list_count = web_state_list->count();
  if (!web_state_list_count) {
    return;
  }

  // Snapshots used to be identified by the web state stable identifier, but are
  // now identified by a snapshot ID.
  NSMutableArray<NSString*>* stable_identifiers =
      [NSMutableArray arrayWithCapacity:web_state_list_count];

  std::vector<SnapshotID> snapshot_identifiers;
  snapshot_identifiers.reserve(web_state_list_count);

  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [stable_identifiers addObject:web_state->GetStableIdentifier()];
    snapshot_identifiers.push_back(
        SnapshotTabHelper::FromWebState(web_state)->GetSnapshotID());
  }

  [snapshot_storage_ renameSnapshotsWithIDs:stable_identifiers
                                      toIDs:snapshot_identifiers];
}

void SnapshotBrowserAgent::PurgeUnusedSnapshots() {
  DCHECK(snapshot_storage_);
  std::vector<SnapshotID> snapshot_ids = GetSnapshotIDs();
  // Keep snapshots that are less than one minute old, to prevent a concurrency
  // issue if they are created while the purge is running.
  const base::Time one_minute_ago = base::Time::Now() - base::Minutes(1);
  [snapshot_storage_ purgeImagesOlderThan:one_minute_ago keeping:snapshot_ids];
}

std::vector<SnapshotID> SnapshotBrowserAgent::GetSnapshotIDs() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  const int web_state_list_count = web_state_list->count();

  std::vector<SnapshotID> snapshot_ids;
  snapshot_ids.reserve(web_state_list_count);

  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    snapshot_ids.push_back(
        SnapshotTabHelper::FromWebState(web_state)->GetSnapshotID());
  }
  return snapshot_ids;
}
