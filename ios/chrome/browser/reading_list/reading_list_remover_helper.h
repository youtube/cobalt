// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_REMOVER_HELPER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"

class ChromeBrowserState;
class ReadingListDownloadService;

namespace reading_list {

// Helper class to asynchronously remove reading list entries.
class ReadingListRemoverHelper : public ReadingListModelObserver {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  explicit ReadingListRemoverHelper(ChromeBrowserState* browser_state);

  ReadingListRemoverHelper(const ReadingListRemoverHelper&) = delete;
  ReadingListRemoverHelper& operator=(const ReadingListRemoverHelper&) = delete;

  ~ReadingListRemoverHelper() override;

  // Removes all Reading list items and asynchronously invoke `completion` with
  // boolean indicating success or failure.
  void RemoveAllUserReadingListItemsIOS(Callback completion);

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;

 private:
  // Invoked when the reading list items have been deleted. Invoke the
  // completion callback with `success` (invocation is asynchronous so
  // the object won't be deleted immediately).
  void ReadlingListItemsRemoved(bool success);

  Callback completion_;
  ReadingListModel* reading_list_model_ = nullptr;
  ReadingListDownloadService* reading_list_download_service_ = nullptr;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      scoped_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_REMOVER_HELPER_H_
