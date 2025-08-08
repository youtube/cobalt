// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/data_sharing/test_support/fake_data_sharing_sdk_delegate.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

namespace {

using base::test::RunClosure;
using testing::Eq;

const char kGroupId[] = "/?-group_id";
const char kEncodedGroupId[] = "%2F%3F-group_id";
const char kTokenBlob[] = "/?-_token";
const char kEncodedTokenBlob[] = "%2F%3F-_token";

sync_pb::CollaborationGroupSpecifics MakeCollaborationGroupSpecifics(
    const GroupId& id) {
  sync_pb::CollaborationGroupSpecifics result;
  result.set_collaboration_id(id.value());
  result.set_changed_at_timestamp_millis_since_unix_epoch(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  return result;
}

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_collaboration_group() = specifics;
  entity_data.name = specifics.collaboration_id();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> EntityChangeAddFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateAdd(specifics.collaboration_id(),
                                         EntityDataFromSpecifics(specifics));
}

MATCHER_P(HasDisplayName, expected_name, "") {
  return arg.display_name == expected_name;
}

// Enum cases for parameterizing the tests.
enum Variant {
  kDelegateAtCreation,
  kDelegateAfter,
};

}  // namespace

class DataSharingServiceImplTest : public ::testing::TestWithParam<Variant> {
 public:
  DataSharingServiceImplTest() = default;

  ~DataSharingServiceImplTest() override { task_environment_.RunUntilIdle(); }

  void SetUp() override {
    EXPECT_TRUE(profile_dir_.CreateUniqueTempDir());

    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    std::unique_ptr<FakeDataSharingSDKDelegate> sdk_delegate;
    if (!ShouldSetSDKLater()) {
      sdk_delegate = std::make_unique<FakeDataSharingSDKDelegate>();
      not_owned_sdk_delegate_ = sdk_delegate.get();
    }

    data_sharing_service_ = std::make_unique<DataSharingServiceImpl>(
        profile_dir_.GetPath(),
        std::move(test_url_loader_factory),
        identity_test_env_.identity_manager(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        version_info::Channel::UNKNOWN, std::move(sdk_delegate),
        /*ui_delegate=*/nullptr);

    if (ShouldSetSDKLater()) {
      sdk_delegate = std::make_unique<FakeDataSharingSDKDelegate>();
      not_owned_sdk_delegate_ = sdk_delegate.get();
      data_sharing_service_->SetSDKDelegate(std::move(sdk_delegate));
    }
  }

 protected:
  // Returns whether the SDK delegate should be set after the DataSharingService
  // creation.
  bool ShouldSetSDKLater() {
    switch (GetParam()) {
      case kDelegateAtCreation:
        return false;
      case kDelegateAfter:
        return true;
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir profile_dir_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DataSharingServiceImpl> data_sharing_service_;
  raw_ptr<FakeDataSharingSDKDelegate> not_owned_sdk_delegate_;
};

TEST_P(DataSharingServiceImplTest, ConstructionAndEmptyServiceCheck) {
  EXPECT_FALSE(data_sharing_service_->IsEmptyService());
}

TEST_P(DataSharingServiceImplTest, GetDataSharingNetworkLoader) {
  EXPECT_TRUE(data_sharing_service_->GetDataSharingNetworkLoader());
}

TEST_P(DataSharingServiceImplTest, ShouldCreateGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const std::string display_name = "display_name";

  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->CreateGroup(
      display_name,
      base::BindLambdaForTesting(
          [&run_loop, &outcome](
              const DataSharingService::GroupDataOrFailureOutcome& result) {
            outcome = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->display_name, Eq(display_name));

  ASSERT_TRUE(not_owned_sdk_delegate_->GetGroup(outcome->group_token.group_id)
                  .has_value());
  EXPECT_THAT(not_owned_sdk_delegate_->GetGroup(outcome->group_token.group_id)
                  ->display_name(),
              Eq(display_name));
}

TEST_P(DataSharingServiceImplTest, ShouldDeleteGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");
  ASSERT_TRUE(not_owned_sdk_delegate_->GetGroup(group_id).has_value());

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->DeleteGroup(group_id, callback.Get());
  run_loop.Run();

  EXPECT_FALSE(not_owned_sdk_delegate_->GetGroup(group_id).has_value());
}

TEST_P(DataSharingServiceImplTest, ShouldReadGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const std::string display_name = "display_name";
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name);

  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadGroup(
      group_id,
      base::BindLambdaForTesting(
          [&run_loop, &outcome](
              const DataSharingService::GroupDataOrFailureOutcome& result) {
            outcome = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->display_name, Eq(display_name));
  EXPECT_THAT(outcome->group_token.group_id, Eq(group_id));
}

TEST_P(DataSharingServiceImplTest, ShouldReadAllGroups) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  // Delegate stores 2 groups.
  const std::string display_name1 = "group1";
  const GroupId group_id1 =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name1);
  const std::string display_name2 = "group2";
  const GroupId group_id2 =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name2);

  // Mimics initial sync for collaboration group datatype with the same two
  // groups.
  auto* collaboration_group_bridge =
      data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeCollaborationGroupSpecifics(group_id1)));
  entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeCollaborationGroupSpecifics(group_id2)));

  collaboration_group_bridge->MergeFullSyncData(
      collaboration_group_bridge->CreateMetadataChangeList(),
      std::move(entity_changes));

  // Verify that DataSharingService reads the same groups.
  DataSharingService::GroupsDataSetOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadAllGroups(base::BindLambdaForTesting(
      [&run_loop, &outcome](
          const DataSharingService::GroupsDataSetOrFailureOutcome& result) {
        outcome = result;
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->size(), Eq(2));
  const GroupData& group1 = *outcome->begin();
  const GroupData& group2 = *(++outcome->begin());
  EXPECT_THAT(group1.display_name, Eq(display_name1));
  EXPECT_THAT(group1.group_token.group_id, Eq(group_id1));
  EXPECT_THAT(group2.display_name, Eq(display_name2));
  EXPECT_THAT(group2.group_token.group_id, Eq(group_id2));
}

TEST_P(DataSharingServiceImplTest, ShouldInviteMember) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy paths.
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const std::string gaia_id = "123456789";
  not_owned_sdk_delegate_->AddAccount(email, gaia_id);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->InviteMember(group_id, email, callback.Get());
  run_loop.Run();

  auto group = not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group.has_value());
  ASSERT_THAT(group->members().size(), Eq(1));
  EXPECT_THAT(group->members(0).gaia_id(), Eq(gaia_id));
}

TEST_P(DataSharingServiceImplTest, ShouldRemoveMember) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy paths.
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const std::string gaia_id = "123456789";
  not_owned_sdk_delegate_->AddAccount(email, gaia_id);
  not_owned_sdk_delegate_->AddMember(group_id, gaia_id);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->RemoveMember(group_id, email, callback.Get());
  run_loop.Run();

  auto group = not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group.has_value());
  EXPECT_TRUE(group->members().empty());
}

TEST_P(DataSharingServiceImplTest, ShouldLeaveGroup) {
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const std::string gaia_id = "123456789";
  not_owned_sdk_delegate_->SetUserGaiaId(gaia_id);
  not_owned_sdk_delegate_->AddAccount(email, gaia_id);
  not_owned_sdk_delegate_->AddMember(group_id, gaia_id);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->LeaveGroup(group_id, callback.Get());
  run_loop.Run();

  auto group = not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group.has_value());
  EXPECT_TRUE(group->members().empty());
}

TEST_P(DataSharingServiceImplTest, ParseAndInterceptDataSharingURL) {
  GroupData group_data = GroupData();
  group_data.group_token =
      GroupToken(data_sharing::GroupId(kGroupId), kTokenBlob);
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?group_id=" + kGroupId + "&token_blob=" + kTokenBlob);

  DataSharingService::ParseUrlResult result =
      data_sharing_service_->ParseDataSharingUrl(url);

  // Verify valid path.
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(group_data.group_token.group_id.value(),
            result.value().group_id.value());
  EXPECT_EQ(group_data.group_token.access_token, result.value().access_token);
  EXPECT_TRUE(data_sharing_service_->ShouldInterceptNavigationForShareURL(url));

  // Verify host/path error.
  std::string invalid = "https://www.test.com/";
  url = GURL(invalid + "?group_id=" + kGroupId + "&token_blob=" + kTokenBlob);
  result = data_sharing_service_->ParseDataSharingUrl(url);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            DataSharingService::ParseUrlStatus::kHostOrPathMismatchFailure);
  EXPECT_FALSE(
      data_sharing_service_->ShouldInterceptNavigationForShareURL(url));

  // Verify query missing error.
  url = GURL(data_sharing::features::kDataSharingURL.Get() +
             "?group_id=" + kGroupId);
  result = data_sharing_service_->ParseDataSharingUrl(url);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            DataSharingService::ParseUrlStatus::kQueryMissingFailure);
  EXPECT_FALSE(
      data_sharing_service_->ShouldInterceptNavigationForShareURL(url));
}

TEST_P(DataSharingServiceImplTest, GetDataSharingUrl) {
  GroupData group_data = GroupData();
  group_data.group_token =
      GroupToken(data_sharing::GroupId(kGroupId), kTokenBlob);
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() + "?group_id=" +
                  kEncodedGroupId + "&token_blob=" + kEncodedTokenBlob);

  std::unique_ptr<GURL> result_url =
      data_sharing_service_->GetDataSharingUrl(group_data);

  // Verify valid path.
  EXPECT_TRUE(result_url);
  EXPECT_EQ(url, *result_url);

  // Verify invalid group data.
  result_url = data_sharing_service_->GetDataSharingUrl(GroupData());
  EXPECT_FALSE(result_url);
}

INSTANTIATE_TEST_SUITE_P(DataSharingServiceImplTestInstantiation,
                         DataSharingServiceImplTest,
                         ::testing::Values(Variant::kDelegateAtCreation,
                                           Variant::kDelegateAfter));

}  // namespace data_sharing
