// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/features/reading_list_switches.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/reading_list/features/reading_list_buildflags.h"

namespace reading_list {
namespace switches {

BASE_FEATURE(kReadLaterBackendMigration,
             "ReadLaterBackendMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Feature flag used for enabling read later reminder notification.
BASE_FEATURE(kReadLaterReminderNotification,
             "ReadLaterReminderNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kReadingListEnableDualReadingListModel,
             "ReadingListEnableDualReadingListModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             "ReadingListEnableSyncTransportModeUponSignIn",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsReadingListAccountStorageUIEnabled() {
  return base::FeatureList::IsEnabled(kReadingListEnableDualReadingListModel) &&
         base::FeatureList::IsEnabled(
             kReadingListEnableSyncTransportModeUponSignIn);
}

}  // namespace switches
}  // namespace reading_list
