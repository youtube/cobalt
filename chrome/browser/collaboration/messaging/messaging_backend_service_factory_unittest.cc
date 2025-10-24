// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration::messaging {
namespace {

class MessagingBackendServiceFactoryTest : public testing::Test {
 protected:
  MessagingBackendServiceFactoryTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{data_sharing::features::kDataSharingFeature, true},
         // Required for desktop platforms.
         {tab_groups::kTabGroupSyncServiceDesktopMigration, true},
         // Required for Android.
         {tab_groups::kTabGroupSyncAndroid, true}});
  }

  ~MessagingBackendServiceFactoryTest() override = default;

  void SetUp() override { profile_ = TestingProfile::Builder().Build(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MessagingBackendServiceFactoryTest, ServiceCreatedInRegularProfile) {
  MessagingBackendService* service =
      MessagingBackendServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service);
}

TEST_F(MessagingBackendServiceFactoryTest, ServiceNotCreatedInIncognito) {
  Profile* otr_profile = profile_.get()->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_FALSE(MessagingBackendServiceFactory::GetForProfile(otr_profile));
}

}  // namespace
}  // namespace collaboration::messaging
