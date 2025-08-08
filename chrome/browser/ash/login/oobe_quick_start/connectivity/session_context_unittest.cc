// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"

#include <string>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

namespace {

// The keys expected in the dict returned by PrepareForUpdate()
constexpr char kPrepareForUpdateSessionIdKey[] = "session_id";
constexpr char kPrepareForUpdateAdvertisingIdKey[] = "advertising_id";
constexpr char kPrepareForUpdateSecondarySharedSecretKey[] =
    "secondary_shared_secret";

}  // namespace

class SessionContextTest : public testing::Test {
 public:
  SessionContextTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  SessionContextTest(const SessionContextTest&) = delete;
  SessionContextTest& operator=(const SessionContextTest&) = delete;

  void SetUp() override {
    session_context_ = std::make_unique<SessionContext>();
  }

  PrefService* GetLocalState() { return local_state_.Get(); }

  std::string GetSecondarySharedSecretString() {
    SessionContext::SharedSecret secondary_shared_secret =
        session_context_->secondary_shared_secret();
    std::string secondary_shared_secret_bytes(secondary_shared_secret.begin(),
                                              secondary_shared_secret.end());
    std::string secondary_shared_secret_base64;
    base::Base64Encode(secondary_shared_secret_bytes,
                       &secondary_shared_secret_base64);
    return secondary_shared_secret_base64;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<SessionContext> session_context_;
  ScopedTestingLocalState local_state_;
};

TEST_F(SessionContextTest, GetPrepareForUpdateInfo) {
  base::Value::Dict prepare_for_update_info =
      session_context_->GetPrepareForUpdateInfo();
  EXPECT_FALSE(prepare_for_update_info.empty());
  EXPECT_EQ(base::NumberToString(session_context_->session_id()),
            *prepare_for_update_info.FindString(kPrepareForUpdateSessionIdKey));
  EXPECT_EQ(
      session_context_->advertising_id().ToString(),
      *prepare_for_update_info.FindString(kPrepareForUpdateAdvertisingIdKey));
  EXPECT_EQ(GetSecondarySharedSecretString(),
            *prepare_for_update_info.FindString(
                kPrepareForUpdateSecondarySharedSecretKey));
}

TEST_F(SessionContextTest, ResumeAfterUpdate) {
  // The bootstrap controller expects these prefs to be set if resuming after an
  // update.
  GetLocalState()->SetBoolean(prefs::kShouldResumeQuickStartAfterReboot, true);
  GetLocalState()->SetDict(prefs::kResumeQuickStartAfterRebootInfo,
                           session_context_->GetPrepareForUpdateInfo());

  SessionContext::SessionId expected_session_id =
      session_context_->session_id();
  std::string expected_advertising_id =
      session_context_->advertising_id().ToString();
  SessionContext::SharedSecret expected_shared_secret =
      session_context_->secondary_shared_secret();

  // To simulate "update" behavior, re-instantiate |session_context| with proper
  // local state prefs set.
  session_context_ = std::make_unique<SessionContext>();

  ASSERT_EQ(expected_session_id, session_context_->session_id());
  ASSERT_EQ(expected_advertising_id,
            session_context_->advertising_id().ToString());
  ASSERT_EQ(expected_shared_secret, session_context_->shared_secret());
  // Prefs should be cleared after the |bootstrap_controller_| construction.
  ASSERT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  ASSERT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
}

}  // namespace ash::quick_start
