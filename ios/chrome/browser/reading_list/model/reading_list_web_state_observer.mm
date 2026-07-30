// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_web_state_observer.h"

#import "base/check.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

ReadingListWebStateObserver::~ReadingListWebStateObserver() {
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
    reading_list_model_ = nullptr;
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

ReadingListWebStateObserver::ReadingListWebStateObserver(
    web::WebState* web_state,
    ReadingListModel* reading_list_model)
    : web_state_(web_state), reading_list_model_(reading_list_model) {
  web_state_->AddObserver(this);
  reading_list_model_->AddObserver(this);
}

void ReadingListWebStateObserver::ReadingListModelLoaded(
    const ReadingListModel* model) {
  // Nothing to do.
}

void ReadingListWebStateObserver::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK_EQ(reading_list_model_, model);
  reading_list_model_ = nullptr;
  web_state_->RemoveUserData(UserDataKey());
}

void ReadingListWebStateObserver::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    const GURL& url = web_state->GetLastCommittedURL();
    if (url.is_valid() && reading_list_model_ &&
        reading_list_model_->loaded()) {
      reading_list_model_->SetReadStatusIfExists(url, true);
    }
  }
}

void ReadingListWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveUserData(UserDataKey());
}
