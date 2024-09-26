// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_BLOCK_SITE_TASK_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_BLOCK_SITE_TASK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/offline_pages/task/task.h"

using offline_pages::Task;

namespace explore_sites {

// Takes a URL that the user has asked us to remove, and adds it to a blocklist
// of sites we will stop showing in Explore on Sites.
class BlockSiteTask : public Task {
 public:
  BlockSiteTask(ExploreSitesStore* store, std::string url);
  ~BlockSiteTask() override;

  bool complete() const { return complete_; }
  bool result() const { return result_; }

 private:
  // Task implementation:
  void Run() override;

  void FinishedExecuting(bool result);

  raw_ptr<ExploreSitesStore> store_;  // outlives this class.
  std::string url_;

  bool complete_ = false;
  bool result_ = false;
  BooleanCallback callback_;

  base::WeakPtrFactory<BlockSiteTask> weak_ptr_factory_{this};
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_BLOCK_SITE_TASK_H_
