// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

struct ZeroTouchParam {
  const char* enable_zero_touch_flag;
  EnrollmentConfig::AuthMechanism auth_mechanism;
  EnrollmentConfig::AuthMechanism auth_mechanism_after_oobe;

  ZeroTouchParam(const char* flag,
                 EnrollmentConfig::AuthMechanism auth,
                 EnrollmentConfig::AuthMechanism auth_after_oobe)
      : enable_zero_touch_flag(flag),
        auth_mechanism(auth),
        auth_mechanism_after_oobe(auth_after_oobe) {}
};

class EnrollmentConfigTest : public testing::TestWithParam<ZeroTouchParam> {
 protected:
  EnrollmentConfigTest() {
    RegisterLocalState(local_state_.registry());
    statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, "fake-serial");
    statistics_provider_.SetMachineStatistic(ash::system::kHardwareClassKey,
                                             "fake-hardware");
  }

  void SetupZeroTouchCommandLineSwitch();

  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  TestingPrefServiceSimple local_state_;
  ash::StubInstallAttributes install_attributes_;
};

void EnrollmentConfigTest::SetupZeroTouchCommandLineSwitch() {
  const ZeroTouchParam& param = GetParam();
  if (param.enable_zero_touch_flag != nullptr) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableZeroTouchEnrollment,
        param.enable_zero_touch_flag);
  }
}

TEST_P(EnrollmentConfigTest, GetPrescribedEnrollmentConfigDuringOOBE) {
  SetupZeroTouchCommandLineSwitch();

  // Default configuration is empty.
  EnrollmentConfig config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Set signals in increasing order of precedence, check results.

  // OEM manifest: advertised enrollment.
  statistics_provider_.SetMachineFlag(ash::system::kOemIsEnterpriseManagedKey,
                                      true);
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Pref: advertised enrollment. The resulting |config| is indistinguishable
  // from the OEM manifest configuration, so clear the latter to at least
  // verify the pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      ash::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Server-backed state: advertised enrollment.
  base::Value::Dict state_dict;
  state_dict.Set(kDeviceStateMode,
                 kDeviceStateRestoreModeReEnrollmentRequested);
  state_dict.Set(kDeviceStateManagementDomain, "example.com");
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_SERVER_ADVERTISED, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // OEM manifest: forced enrollment.
  statistics_provider_.SetMachineFlag(ash::system::kOemIsEnterpriseManagedKey,
                                      true);
  statistics_provider_.SetMachineFlag(
      ash::system::kOemCanExitEnterpriseEnrollmentKey, false);
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Pref: forced enrollment. The resulting |config| is indistinguishable from
  // the OEM manifest configuration, so clear the latter to at least verify the
  // pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      ash::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Server-backed state: forced enrollment.
  state_dict.Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentEnforced);
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_SERVER_FORCED, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);
}

TEST_P(EnrollmentConfigTest, GetPrescribedEnrollmentConfigAfterOOBE) {
  SetupZeroTouchCommandLineSwitch();

  // If OOBE is complete, we may re-enroll to the domain configured in install
  // attributes. This is only enforced after detecting enrollment loss.
  local_state_.SetBoolean(ash::prefs::kOobeComplete, true);
  EnrollmentConfig config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);

  // Advertised enrollment gets ignored.
  local_state_.SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  statistics_provider_.SetMachineFlag(ash::system::kOemIsEnterpriseManagedKey,
                                      true);
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);

  // If the device is enterprise-managed, the management domain gets pulled from
  // install attributes.
  install_attributes_.SetCloudManaged("example.com", "fake-id");
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);

  // If enrollment recovery is on, this is signaled in |config.mode|.
  local_state_.SetBoolean(prefs::kEnrollmentRecoveryRequired, true);
  config = EnrollmentConfig::GetPrescribedEnrollmentConfig(
      local_state_, install_attributes_, &statistics_provider_);
  EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);
}

INSTANTIATE_TEST_SUITE_P(
    ZeroTouchFlag,
    EnrollmentConfigTest,
    ::testing::Values(
        ZeroTouchParam(nullptr,  // No flag set.
                       EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE,
                       EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE),
        ZeroTouchParam("",  // Flag set without a set value.
                       EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE,
                       EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE),
        ZeroTouchParam("forced",
                       EnrollmentConfig::AUTH_MECHANISM_ATTESTATION,
                       EnrollmentConfig::AUTH_MECHANISM_ATTESTATION)));

}  // namespace policy
