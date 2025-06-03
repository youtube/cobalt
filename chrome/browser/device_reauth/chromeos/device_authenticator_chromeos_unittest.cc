// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_authenticator_chromeos.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/device_reauth/device_reauth_metrics_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using device_reauth::DeviceAuthenticator;
using device_reauth::ReauthResult;

class MockSystemAuthenticator : public AuthenticatorChromeOSInterface {
 public:
  MOCK_METHOD(void,
              AuthenticateUser,
              (base::OnceCallback<void(bool)> callback),
              (override));
};

constexpr base::TimeDelta kAuthValidityPeriod = base::Seconds(60);
constexpr char kHistogramName[] =
    "PasswordManager.ReauthToAccessPasswordInSettings";

}  // namespace

class DeviceAuthenticatorChromeOSTest : public testing::Test {
 public:
  DeviceAuthenticatorChromeOSTest()
      : device_authenticator_params_(
            kAuthValidityPeriod,
            device_reauth::DeviceAuthSource::kPasswordManager,
            kHistogramName) {}
  void SetUp() override {
    std::unique_ptr<MockSystemAuthenticator> system_authenticator =
        std::make_unique<MockSystemAuthenticator>();
    system_authenticator_ = system_authenticator.get();
    authenticator_ = std::make_unique<DeviceAuthenticatorChromeOS>(
        std::move(system_authenticator), &proxy_, device_authenticator_params_);
  }

  DeviceAuthenticatorChromeOS* authenticator() { return authenticator_.get(); }

  MockSystemAuthenticator& system_authenticator() {
    return *system_authenticator_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void ExpectAuthenticationAndSetResult(bool result) {
    EXPECT_CALL(system_authenticator(), AuthenticateUser)
        .WillOnce(testing::WithArg<0>([result](auto callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), /*auth_succeeded=*/result));
        }));
  }

 private:
  DeviceAuthenticatorProxy proxy_;
  device_reauth::DeviceAuthParams device_authenticator_params_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<DeviceAuthenticatorChromeOS> authenticator_;
  base::HistogramTester histogram_tester_;

  // This is owned by the authenticator.
  raw_ptr<MockSystemAuthenticator> system_authenticator_ = nullptr;
};

// If time that passed since the last successful authentication is smaller than
// kAuthValidityPeriod, no reauthentication is needed.
TEST_F(DeviceAuthenticatorChromeOSTest,
       NoReauthenticationIfLessThanAuthValidityPeriod) {
  ExpectAuthenticationAndSetResult(true);
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  // The delay is smaller than kAuthValidityPeriod there shouldn't be
  // another prompt, so the auth should be reported as successful.
  task_environment().FastForwardBy(kAuthValidityPeriod / 2);

  EXPECT_CALL(system_authenticator(), AuthenticateUser).Times(0);
  base::MockCallback<DeviceAuthenticator::AuthenticateCallback> result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback.Get());

  task_environment().RunUntilIdle();
}

// If the time since the last reauthentication is greater than
// kAuthValidityPeriod reauthentication is needed.
TEST_F(DeviceAuthenticatorChromeOSTest, ReauthenticationIfMoreThan60Seconds) {
  // Simulate a previous successful authentication
  ExpectAuthenticationAndSetResult(true);
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  task_environment().FastForwardBy(kAuthValidityPeriod * 2);

  // The next call to `Authenticate()` should re-trigger an authentication.
  ExpectAuthenticationAndSetResult(false);
  base::MockCallback<DeviceAuthenticator::AuthenticateCallback> result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback.Get());

  task_environment().RunUntilIdle();
}

// If previous authentication failed, kAuthValidityPeriod isn't started and
// reauthentication will be needed.
TEST_F(DeviceAuthenticatorChromeOSTest, ReauthenticationIfPreviousFailed) {
  ExpectAuthenticationAndSetResult(false);
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  task_environment().RunUntilIdle();

  // The next call to `Authenticate()` should re-trigger an authentication.
  ExpectAuthenticationAndSetResult(true);
  base::MockCallback<DeviceAuthenticator::AuthenticateCallback> result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback.Get());

  task_environment().RunUntilIdle();
}

TEST_F(DeviceAuthenticatorChromeOSTest, RecordSuccessAuthHistogram) {
  ExpectAuthenticationAndSetResult(true);

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  task_environment().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(kHistogramName, ReauthResult::kSuccess,
                                        1);
}

TEST_F(DeviceAuthenticatorChromeOSTest, RecordSkippedAuthHistogram) {
  ExpectAuthenticationAndSetResult(true);

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  task_environment().RunUntilIdle();
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  task_environment().RunUntilIdle();

  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSkipped,
                                       1);
}

TEST_F(DeviceAuthenticatorChromeOSTest, RecordFailAuthHistogram) {
  ExpectAuthenticationAndSetResult(false);

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  task_environment().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(kHistogramName, ReauthResult::kFailure,
                                        1);
}
