// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"

namespace ash::phonehub {

class FeatureStatusProvider;

// UserActionRecorder implementation which generates engagement metrics for
// Phone Hub.
class UserActionRecorderImpl : public UserActionRecorder {
 public:
  explicit UserActionRecorderImpl(
      FeatureStatusProvider* feature_status_provider);
  ~UserActionRecorderImpl() override;

 private:
  friend class UserActionRecorderImplTest;
  FRIEND_TEST_ALL_PREFIXES(UserActionRecorderImplTest, RecordActions);

  // Types of user actions; numerical value should not be reused or reordered
  // since this enum is used in metrics. Keep in sync with PhoneHubUserAction
  // enum in //tools/metrics/histograms/metadata/phonehub/enums.xml.
  //
  // LINT.IfChange(PhoneHubUserAction)
  enum class UserAction {
    kUiOpened = 0,
    kTether = 1,
    kDnd = 2,
    kFindMyDevice = 3,
    kBrowserTab = 4,
    kNotificationDismissal = 5,
    kNotificationReply = 6,
    kCameraRollDownload = 7,
    kAppStreamLauncherOpened = 8,
    kMaxValue = kAppStreamLauncherOpened,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/phonehub/enums.xml:PhoneHubUserAction)

  // UserActionRecorder:
  void RecordUiOpened() override;
  void RecordTetherConnectionAttempt() override;
  void RecordDndAttempt() override;
  void RecordFindMyDeviceAttempt() override;
  void RecordBrowserTabOpened() override;
  void RecordNotificationDismissAttempt() override;
  void RecordNotificationReplyAttempt() override;
  void RecordCameraRollDownloadAttempt() override;
  void RecordAppStreamLauncherOpened() override;

  void HandleUserAction(UserAction action);

  raw_ptr<FeatureStatusProvider> feature_status_provider_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_IMPL_H_
