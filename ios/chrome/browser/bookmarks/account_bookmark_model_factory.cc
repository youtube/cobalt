// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/account_bookmark_model_factory.h"

#include <utility>
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/storage_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/account_bookmark_sync_service_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_client_impl.h"
#include "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildBookmarkModel(web::BrowserState* context) {
  if (!base::FeatureList::IsEnabled(
          bookmarks::kEnableBookmarksAccountStorage)) {
    return nullptr;
  }
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  // Using nullptr for `ManagedBookmarkService`, since managed bookmarks affect
  // only the local bookmark storage.
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model(
      new bookmarks::BookmarkModel(std::make_unique<BookmarkClientImpl>(
          browser_state, /*managed_bookmark_service=*/nullptr,
          ios::AccountBookmarkSyncServiceFactory::GetForBrowserState(
              browser_state))));
  bookmark_model->Load(browser_state->GetStatePath(),
                       bookmarks::StorageType::kAccount);
  // TODO(crbug.com/1424820): Support undo/redo operations.
  return bookmark_model;
}

}  // namespace

// static
bookmarks::BookmarkModel* AccountBookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::BookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AccountBookmarkModelFactory* AccountBookmarkModelFactory::GetInstance() {
  static base::NoDestructor<AccountBookmarkModelFactory> instance;
  return instance.get();
}

// static
AccountBookmarkModelFactory::TestingFactory
AccountBookmarkModelFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildBookmarkModel);
}

AccountBookmarkModelFactory::AccountBookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountBookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  // Bookmark-related prefs are registered by the LocalOrSyncable factory.
  DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
  DependsOn(ios::AccountBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
}

AccountBookmarkModelFactory::~AccountBookmarkModelFactory() = default;

std::unique_ptr<KeyedService>
AccountBookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildBookmarkModel(context);
}

web::BrowserState* AccountBookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
