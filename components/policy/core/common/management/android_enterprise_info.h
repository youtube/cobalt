// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_ANDROID_ENTERPRISE_INFO_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_ANDROID_ENTERPRISE_INFO_H_

#include <queue>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/policy/policy_export.h"

// Class to connect native calls to
// org.chromium.components.policy.EnterpriseInfo. This class is
// only usable for Android and is only built for Android.

// Only use from the UI Thread.

namespace policy {

class AndroidEnterpriseInfoFriendHelper;

class POLICY_EXPORT AndroidEnterpriseInfo {
 public:
  // Callback invoked with (bool device_owned, bool profile_owned).
  using EnterpriseInfoCallback = base::OnceCallback<void(bool, bool)>;
  ~AndroidEnterpriseInfo();

  static AndroidEnterpriseInfo* GetInstance();

  // Request the owned state from
  // org.chromium.components.policy.EnterpriseInfo and notify
  // |callback| when the request is complete. |callback| is added to a list of
  // callbacks and they are notified in the order they were received. Use from
  // the UI thread.
  void GetAndroidEnterpriseInfoState(EnterpriseInfoCallback callback);

  void set_skip_jni_call_for_testing(bool value) {
    skip_jni_call_for_testing_ = value;
  }
  void ServiceCallbacksForTesting(bool device_owned, bool profile_owned) {
    ServiceCallbacks(device_owned, profile_owned);
  }

 private:
  friend base::NoDestructor<AndroidEnterpriseInfo>;
  friend AndroidEnterpriseInfoFriendHelper;

  AndroidEnterpriseInfo();

  AndroidEnterpriseInfo(const AndroidEnterpriseInfo&) = delete;
  AndroidEnterpriseInfo& operator=(const AndroidEnterpriseInfo&) = delete;
  AndroidEnterpriseInfo(AndroidEnterpriseInfo&&) = delete;
  AndroidEnterpriseInfo& operator=(AndroidEnterpriseInfo&&) = delete;

  // This function is for the Java side code to return its result.
  // Calls are made on the UI thread.
  void ServiceCallbacks(bool device_owned, bool profile_owned);

  std::queue<EnterpriseInfoCallback> callback_queue_;

  bool skip_jni_call_for_testing_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_ANDROID_ENTERPRISE_INFO_H_
