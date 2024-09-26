// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_CLEAR_ACTIVITIES_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_CLEAR_ACTIVITIES_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Takes a URL that the user has asked us to remove, and adds it to a blocklist
// of sites we will stop showing in Explore on Sites.
class ClearActivitiesTask : public Task {
 public:
  ClearActivitiesTask(ExploreSitesStore* store,
                      base::Time begin,
                      base::Time end,
                      BooleanCallback callback);
  ~ClearActivitiesTask() override;

 private:
  // Task implementation:
  void Run() override;

  void DoneExecuting(bool result);

  raw_ptr<ExploreSitesStore> store_;  // outlives this class.
  base::Time begin_;
  base::Time end_;
  BooleanCallback callback_;
  base::WeakPtrFactory<ClearActivitiesTask> weak_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_CLEAR_ACTIVITIES_TASK_H_
