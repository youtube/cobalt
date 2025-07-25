// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
#define CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

bool OtherAppUsageStatsAllowed(const std::vector<std::string>& app_ids,
                               UpdaterScope scope);

class PersistedData;

class UpdateUsageStatsTask
    : public base::RefCountedThreadSafe<UpdateUsageStatsTask> {
 public:
  UpdateUsageStatsTask(UpdaterScope scope,
                       scoped_refptr<PersistedData> persisted_data);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<UpdateUsageStatsTask>;
  FRIEND_TEST_ALL_PREFIXES(UpdateUsageStatsTaskTest, NoApps);
  FRIEND_TEST_ALL_PREFIXES(UpdateUsageStatsTaskTest, OneAppEnabled);
  FRIEND_TEST_ALL_PREFIXES(UpdateUsageStatsTaskTest, ZeroAppsEnabled);
  virtual ~UpdateUsageStatsTask();

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  scoped_refptr<PersistedData> persisted_data_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
