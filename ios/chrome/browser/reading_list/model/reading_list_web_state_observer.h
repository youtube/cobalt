// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_WEB_STATE_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#import "components/reading_list/core/reading_list_model_observer.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

class ReadingListModel;

// Observes the loading of pages, and marks matching reading list entries as
// read when they successfully load.
class ReadingListWebStateObserver
    : public ReadingListModelObserver,
      public web::WebStateObserver,
      public web::WebStateUserData<ReadingListWebStateObserver> {
 public:
  ReadingListWebStateObserver(const ReadingListWebStateObserver&) = delete;
  ReadingListWebStateObserver& operator=(const ReadingListWebStateObserver&) =
      delete;

  ~ReadingListWebStateObserver() override;

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;

 private:
  friend class web::WebStateUserData<ReadingListWebStateObserver>;

  ReadingListWebStateObserver(web::WebState* web_state,
                              ReadingListModel* reading_list_model);

  // WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  raw_ptr<ReadingListModel> reading_list_model_;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_WEB_STATE_OBSERVER_H_
