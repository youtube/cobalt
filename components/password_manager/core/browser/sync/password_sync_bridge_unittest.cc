// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include <memory>
#include <random>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;

constexpr char kSignonRealm1[] = "abc";
constexpr char kSignonRealm2[] = "def";
constexpr char kSignonRealm3[] = "xyz";
constexpr time_t kIssuesCreationTime = 10;

// |*arg| must be of type EntityData.
MATCHER_P(EntityDataHasSignonRealm, expected_signon_realm, "") {
  return arg->specifics.password()
             .client_only_encrypted_data()
             .signon_realm() == expected_signon_realm;
}

bool PasswordIssuesHasExpectedInsecureTypes(
    const sync_pb::PasswordIssues& issues,
    const std::vector<InsecureType>& expected_types) {
  return base::ranges::all_of(expected_types, [&issues](auto type) {
    switch (type) {
      case InsecureType::kLeaked:
        return issues.has_leaked_password_issue();
      case InsecureType::kPhished:
        return issues.has_phished_password_issue();
      case InsecureType::kWeak:
        return issues.has_weak_password_issue();
      case InsecureType::kReused:
        return issues.has_reused_password_issue();
    }
  });
}

MATCHER_P(EntityDataHasSecurityIssueTypes, expected_issue_types, "") {
  const auto& password_issues_data =
      arg->specifics.password().client_only_encrypted_data().password_issues();
  return PasswordIssuesHasExpectedInsecureTypes(password_issues_data,
                                                expected_issue_types);
}

// |*arg| must be of type sync_pb::PasswordSpecificsData.
MATCHER_P(FormHasSignonRealm, expected_signon_realm, "") {
  return arg.signon_realm() == expected_signon_realm;
}

// |*arg| must be of type sync_pb::PasswordSpecificsData..
MATCHER_P(FormHasPasswordIssues, expected_issues, "") {
  return PasswordFromSpecifics(arg).password_issues == expected_issues;
}

// |*arg| must be of type PasswordStoreChange.
MATCHER_P(ChangeHasPrimaryKey, expected_primary_key, "") {
  return arg.form().primary_key.value().value() == expected_primary_key;
}

// |*arg| must be of type SyncMetadataStoreChangeList.
MATCHER_P(IsSyncMetadataStoreChangeListWithStore, expected_metadata_store, "") {
  return static_cast<const syncer::SyncMetadataStoreChangeList*>(arg)
             ->GetMetadataStoreForTesting() == expected_metadata_store;
}

sync_pb::PasswordIssues CreatePasswordIssues(
    const std::vector<InsecureType>& issue_types) {
  sync_pb::PasswordIssues remote_issues;
  for (auto type : issue_types) {
    sync_pb::PasswordIssues_PasswordIssue remote_issue;
    remote_issue.set_date_first_detection_windows_epoch_micros(
        base::Time::FromTimeT(kIssuesCreationTime)
            .ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
    remote_issue.set_is_muted(false);
    switch (type) {
      case InsecureType::kLeaked:
        *remote_issues.mutable_leaked_password_issue() = remote_issue;
        break;
      case InsecureType::kPhished:
        *remote_issues.mutable_phished_password_issue() = remote_issue;
        break;
      case InsecureType::kWeak:
        *remote_issues.mutable_weak_password_issue() = remote_issue;
        break;
      case InsecureType::kReused:
        *remote_issues.mutable_reused_password_issue() = remote_issue;
        break;
    }
  }
  return remote_issues;
}

sync_pb::PasswordSpecifics CreateSpecifics(
    const std::string& origin,
    const std::string& username_element,
    const std::string& username_value,
    const std::string& password_element,
    const std::string& signon_realm,
    const std::vector<InsecureType>& issue_types) {
  sync_pb::EntitySpecifics password_specifics;
  sync_pb::PasswordSpecificsData* password_data =
      password_specifics.mutable_password()
          ->mutable_client_only_encrypted_data();
  password_data->set_origin(origin);
  password_data->set_username_element(username_element);
  password_data->set_username_value(username_value);
  password_data->set_password_element(password_element);
  password_data->set_signon_realm(signon_realm);
  if (!issue_types.empty()) {
    *password_data->mutable_password_issues() =
        CreatePasswordIssues(issue_types);
  }
  return password_specifics.password();
}

sync_pb::PasswordSpecifics CreateSpecificsWithSignonRealm(
    const std::string& signon_realm) {
  return CreateSpecifics("http://www.origin.com/", "username_element",
                         "username_value", "password_element", signon_realm,
                         {});
}

sync_pb::PasswordSpecifics CreateSpecificsWithSignonRealmAndIssues(
    const std::string& signon_realm,
    const std::vector<InsecureType>& issue_types) {
  return CreateSpecifics("http://www.origin.com/", "username_element",
                         "username_value", "password_element", signon_realm,
                         issue_types);
}

PasswordForm MakePasswordFormWithIssues(
    const std::string& signon_realm,
    int primary_key,
    const std::vector<InsecureType>& issue_types) {
  PasswordForm form;
  form.primary_key = FormPrimaryKey(primary_key);
  form.url = GURL("http://www.origin.com");
  form.username_element = u"username_element";
  form.username_value = u"username_value";
  form.password_element = u"password_element";
  form.signon_realm = signon_realm;
  for (const auto& issue_type : issue_types) {
    form.password_issues.insert_or_assign(
        issue_type,
        InsecurityMetadata(base::Time::FromTimeT(kIssuesCreationTime),
                           IsMuted(false), TriggerBackendNotification(false)));
  }
  return form;
}

PasswordForm MakePasswordForm(const std::string& signon_realm,
                              int primary_key = 1) {
  return MakePasswordFormWithIssues(signon_realm, primary_key,
                                    /*issue_types=*/{});
}

PasswordForm MakeBlocklistedForm(const std::string& signon_realm,
                                 int primary_key = 1) {
  PasswordForm form;
  form.primary_key = FormPrimaryKey(primary_key);
  form.url = GURL("http://www.origin.com");
  form.signon_realm = signon_realm;
  form.blocked_by_user = true;
  return form;
}

// A mini database class the supports Add/Update/Remove functionality. It also
// supports an auto increment primary key that starts from 1. It will be used to
// empower the MockPasswordStoreSync be forwarding all database calls to an
// instance of this class.
class FakeDatabase {
 public:
  FakeDatabase() = default;

  FakeDatabase(const FakeDatabase&) = delete;
  FakeDatabase& operator=(const FakeDatabase&) = delete;

  ~FakeDatabase() = default;

  FormRetrievalResult ReadAllCredentials(
      PrimaryKeyToPasswordSpecificsDataMap* map) {
    map->clear();
    for (const auto& [primary_key, form] : data_) {
      map->emplace(
          primary_key,
          std::make_unique<sync_pb::PasswordSpecificsData>(
              SpecificsDataFromPassword(*form, /*base_password_data=*/{})));
    }
    return FormRetrievalResult::kSuccess;
  }

  PasswordStoreChangeList AddCredential(
      const sync_pb::PasswordSpecificsData& specifics,
      AddCredentialError* error) {
    if (error) {
      *error = error_;
    }
    if (error_ == AddCredentialError::kNone) {
      PasswordForm form = PasswordFromSpecifics(specifics);
      form.primary_key = FormPrimaryKey(primary_key_);
      data_[FormPrimaryKey(primary_key_++)] =
          std::make_unique<PasswordForm>(form);
      return {PasswordStoreChange(PasswordStoreChange::ADD, form)};
    }
    return PasswordStoreChangeList();
  }

  PasswordStoreChangeList AddLoginWithPrimaryKey(const PasswordForm& form) {
    FormPrimaryKey form_primary_key(form.primary_key.value());
    DCHECK_EQ(0U, data_.count(form_primary_key));
    data_[form_primary_key] = std::make_unique<PasswordForm>(form);
    return {PasswordStoreChange(PasswordStoreChange::ADD, form)};
  }

  PasswordStoreChangeList UpdateCredential(
      const sync_pb::PasswordSpecificsData& specifics,
      UpdateCredentialError* error) {
    if (error) {
      *error = UpdateCredentialError::kNone;
    }
    PasswordForm form = PasswordFromSpecifics(specifics);
    FormPrimaryKey key = GetPrimaryKey(form);
    form.primary_key = key;
    DCHECK_NE(-1, key.value());
    bool password_changed = data_[key]->password_value != form.password_value;
    // Insecure credentials don't change if neither the form nor the db
    // contain any password_issues.
    bool insecure_changed =
        password_changed ||
        !(form.password_issues.empty() && data_[key]->password_issues.empty());
    data_[key] = std::make_unique<PasswordForm>(form);

    return {PasswordStoreChange(PasswordStoreChange::UPDATE, form,
                                password_changed,
                                InsecureCredentialsChanged(insecure_changed))};
  }

  PasswordStoreChangeList RemoveCredential(FormPrimaryKey key) {
    DCHECK_NE(0U, data_.count(key));
    PasswordForm form = *data_[key];
    data_.erase(key);
    return {PasswordStoreChange(PasswordStoreChange::REMOVE, form)};
  }

  void SetAddLoginError(AddCredentialError error) { error_ = error; }

 private:
  FormPrimaryKey GetPrimaryKey(const PasswordForm& form) const {
    for (const auto& [primary_key, other_form] : data_) {
      if (ArePasswordFormUniqueKeysEqual(*other_form, form)) {
        return primary_key;
      }
    }
    return FormPrimaryKey(-1);
  }

  int primary_key_ = 1;
  std::map<FormPrimaryKey, std::unique_ptr<PasswordForm>> data_;
  AddCredentialError error_ = AddCredentialError::kNone;
};

class MockSyncMetadataStore : public PasswordStoreSync::MetadataStore {
 public:
  MOCK_METHOD(std::unique_ptr<syncer::MetadataBatch>,
              GetAllSyncMetadata,
              (),
              (override));
  MOCK_METHOD(void, DeleteAllSyncMetadata, (), (override));
  MOCK_METHOD(bool,
              UpdateEntityMetadata,
              (syncer::ModelType,
               const std::string&,
               const sync_pb::EntityMetadata&),
              (override));
  MOCK_METHOD(bool,
              ClearEntityMetadata,
              (syncer::ModelType, const std::string&),
              (override));
  MOCK_METHOD(bool,
              UpdateModelTypeState,
              (syncer::ModelType, const sync_pb::ModelTypeState&),
              (override));
  MOCK_METHOD(bool, ClearModelTypeState, (syncer::ModelType), (override));
  MOCK_METHOD(void,
              SetDeletionsHaveSyncedCallback,
              (base::RepeatingCallback<void(bool)>),
              (override));
  MOCK_METHOD(bool, HasUnsyncedDeletions, (), (override));
};

class MockPasswordStoreSync : public PasswordStoreSync {
 public:
  MOCK_METHOD(FormRetrievalResult,
              ReadAllCredentials,
              (PrimaryKeyToPasswordSpecificsDataMap*),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveCredentialByPrimaryKeySync,
              (FormPrimaryKey),
              (override));
  MOCK_METHOD(DatabaseCleanupResult,
              DeleteUndecryptableCredentials,
              (),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              AddCredentialSync,
              (const sync_pb::PasswordSpecificsData&, AddCredentialError*),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              UpdateCredentialSync,
              (const sync_pb::PasswordSpecificsData&, UpdateCredentialError*),
              (override));
  MOCK_METHOD(void,
              NotifyCredentialsChanged,
              (const PasswordStoreChangeList&),
              (override));
  MOCK_METHOD(void, NotifyDeletionsHaveSynced, (bool), (override));
  MOCK_METHOD(void,
              NotifyUnsyncedCredentialsWillBeDeleted,
              (std::vector<PasswordForm>),
              (override));
  MOCK_METHOD(bool, BeginTransaction, (), (override));
  MOCK_METHOD(bool, CommitTransaction, (), (override));
  MOCK_METHOD(void, RollbackTransaction, (), (override));
  MOCK_METHOD(PasswordStoreSync::MetadataStore*,
              GetMetadataStore,
              (),
              (override));
  MOCK_METHOD(bool, IsAccountStore, (), (const override));
  MOCK_METHOD(bool, DeleteAndRecreateDatabaseFile, (), (override));
};

}  // namespace

class PasswordSyncBridgeTest : public testing::Test {
 public:
  PasswordSyncBridgeTest() {
    ON_CALL(mock_password_store_sync_, GetMetadataStore())
        .WillByDefault(testing::Return(&mock_sync_metadata_store_sync_));
    ON_CALL(mock_password_store_sync_, ReadAllCredentials)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::ReadAllCredentials));
    ON_CALL(mock_password_store_sync_, AddCredentialSync)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::AddCredential));
    ON_CALL(mock_password_store_sync_, UpdateCredentialSync)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::UpdateCredential));
    ON_CALL(mock_password_store_sync_, RemoveCredentialByPrimaryKeySync)
        .WillByDefault(Invoke(&fake_db_, &FakeDatabase::RemoveCredential));

    bridge_ = std::make_unique<PasswordSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &mock_password_store_sync_,
        sync_enabled_or_disabled_cb_.Get());

    // It's the responsibility of the PasswordStoreSync to inform the bridge
    // about changes in the password store. The bridge notifies the
    // PasswordStoreSync about the new changes even if they are initiated by the
    // bridge itself.
    ON_CALL(mock_password_store_sync_, NotifyCredentialsChanged)
        .WillByDefault(
            Invoke(bridge(), &PasswordSyncBridge::ActOnPasswordStoreChanges));

    ON_CALL(mock_sync_metadata_store_sync_, GetAllSyncMetadata())
        .WillByDefault(
            []() { return std::make_unique<syncer::MetadataBatch>(); });
    ON_CALL(mock_sync_metadata_store_sync_, UpdateEntityMetadata)
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearEntityMetadata)
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, UpdateModelTypeState)
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearModelTypeState)
        .WillByDefault(testing::Return(true));

    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));
  }

  // Creates an EntityData around a copy of the given specifics.
  syncer::EntityData SpecificsToEntity(
      const sync_pb::PasswordSpecifics& specifics) {
    syncer::EntityData data;
    *data.specifics.mutable_password() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::PASSWORDS, bridge()->GetClientTag(data));
    return data;
  }

  absl::optional<sync_pb::PasswordSpecifics> GetDataFromBridge(
      const std::string& storage_key) {
    std::unique_ptr<syncer::DataBatch> batch;
    bridge_->GetData({storage_key},
                     base::BindLambdaForTesting(
                         [&](std::unique_ptr<syncer::DataBatch> in_batch) {
                           batch = std::move(in_batch);
                         }));
    EXPECT_THAT(batch, NotNull());
    if (!batch || !batch->HasNext()) {
      return absl::nullopt;
    }
    auto [other_storage_key, entity_data] = batch->Next();
    EXPECT_THAT(other_storage_key, Eq(storage_key));
    EXPECT_FALSE(batch->HasNext());
    return entity_data->specifics.password();
  }

  FakeDatabase* fake_db() { return &fake_db_; }

  PasswordSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  MockSyncMetadataStore* mock_sync_metadata_store_sync() {
    return &mock_sync_metadata_store_sync_;
  }

  MockPasswordStoreSync* mock_password_store_sync() {
    return &mock_password_store_sync_;
  }

  base::MockRepeatingClosure* mock_sync_enabled_or_disabled_cb() {
    return &sync_enabled_or_disabled_cb_;
  }

 private:
  FakeDatabase fake_db_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  testing::NiceMock<MockSyncMetadataStore> mock_sync_metadata_store_sync_;
  testing::NiceMock<MockPasswordStoreSync> mock_password_store_sync_;
  testing::NiceMock<base::MockRepeatingClosure> sync_enabled_or_disabled_cb_;
  std::unique_ptr<PasswordSyncBridge> bridge_;
};

TEST_F(PasswordSyncBridgeTest, ShouldComputeClientTagHash) {
  syncer::EntityData data;
  *data.specifics.mutable_password() = CreateSpecifics(
      "http://www.origin.com", "username_element", "username_value",
      "password_element", "signon_realm", /*issue_types=*/{});

  EXPECT_THAT(
      bridge()->GetClientTag(data),
      Eq("http%3A//www.origin.com/"
         "|username_element|username_value|password_element|signon_realm"));
}

TEST_F(PasswordSyncBridgeTest, ShouldForwardLocalChangesToTheProcessor) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));

  PasswordStoreChangeList changes;
  changes.emplace_back(PasswordStoreChange::ADD,
                       MakePasswordForm(kSignonRealm1, 1));
  changes.emplace_back(PasswordStoreChange::UPDATE,
                       MakePasswordForm(kSignonRealm2, 2));
  changes.emplace_back(PasswordStoreChange::REMOVE,
                       MakePasswordForm(kSignonRealm3, 3));
  PasswordStoreSync::MetadataStore* store =
      mock_password_store_sync()->GetMetadataStore();
  EXPECT_CALL(mock_processor(),
              Put("1", EntityDataHasSignonRealm(kSignonRealm1),
                  IsSyncMetadataStoreChangeListWithStore(store)));
  EXPECT_CALL(mock_processor(),
              Put("2", EntityDataHasSignonRealm(kSignonRealm2),
                  IsSyncMetadataStoreChangeListWithStore(store)));
  EXPECT_CALL(mock_processor(),
              Delete("3", IsSyncMetadataStoreChangeListWithStore(store)));

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldNotForwardLocalChangesToTheProcessorIfSyncDisabled) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  PasswordStoreChangeList changes;
  changes.emplace_back(PasswordStoreChange::ADD,
                       MakePasswordForm(kSignonRealm1, 1));
  changes.emplace_back(PasswordStoreChange::UPDATE,
                       MakePasswordForm(kSignonRealm2, 2));
  changes.emplace_back(PasswordStoreChange::REMOVE,
                       MakePasswordForm(kSignonRealm3, 3));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyEmptySyncChangesWithoutError) {
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyMetadataWithEmptySyncChanges) {
  const std::string kStorageKey = "1";
  const std::string kServerId = "TestServerId";
  sync_pb::EntityMetadata metadata;
  metadata.set_server_id(kServerId);
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  metadata_change_list->UpdateMetadata(kStorageKey, metadata);

  EXPECT_CALL(*mock_password_store_sync(), NotifyCredentialsChanged).Times(0);

  EXPECT_CALL(*mock_sync_metadata_store_sync(),
              UpdateEntityMetadata(syncer::PASSWORDS, kStorageKey, _));

  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                            syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyRemoteCreation) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  // Since this remote creation is the first entry in the FakeDatabase, it will
  // be assigned a primary key 1.
  const std::string kStorageKey = "1";

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              AddCredentialSync(FormHasSignonRealm(kSignonRealm1), _));
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kStorageKey, _));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(
      *mock_password_store_sync(),
      NotifyCredentialsChanged(UnorderedElementsAre(ChangeHasPrimaryKey(1))));

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Put).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldIgnoreAndUntrackRemoteCreationWithInvalidData) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  fake_db()->SetAddLoginError(AddCredentialError::kConstraintViolation);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  EXPECT_CALL(mock_processor(),
              UntrackEntityForClientTagHash(
                  SpecificsToEntity(specifics).client_tag_hash));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyRemoteUpdate) {
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  fake_db()->AddLoginWithPrimaryKey(
      MakePasswordForm(kSignonRealm1, kPrimaryKey));

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  testing::InSequence in_sequence;
  base::flat_map<InsecureType, InsecurityMetadata> no_issues;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateCredentialSync(AllOf(FormHasSignonRealm(kSignonRealm1),
                                         FormHasPasswordIssues(no_issues)),
                                   _));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyCredentialsChanged(
                  UnorderedElementsAre(ChangeHasPrimaryKey(kPrimaryKey))));

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), UpdateStorageKey).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      kStorageKey, SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldApplyRemoteDeletion) {
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  fake_db()->AddLoginWithPrimaryKey(
      MakePasswordForm(kSignonRealm1, kPrimaryKey));

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              RemoveCredentialByPrimaryKeySync(FormPrimaryKey(kPrimaryKey)));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyCredentialsChanged(
                  UnorderedElementsAre(ChangeHasPrimaryKey(kPrimaryKey))));

  // Processor shouldn't be notified about remote changes.
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateDelete(kStorageKey));
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldGetDataForStorageKey) {
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";
  PasswordForm form1 = MakePasswordForm(kSignonRealm1, kPrimaryKey1);
  PasswordForm form2 = MakePasswordForm(kSignonRealm2, kPrimaryKey2);

  fake_db()->AddLoginWithPrimaryKey(form1);
  fake_db()->AddLoginWithPrimaryKey(form2);

  absl::optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr1);
  ASSERT_TRUE(optional_specifics.has_value());
  EXPECT_EQ(
      kSignonRealm1,
      optional_specifics.value().client_only_encrypted_data().signon_realm());

  optional_specifics = GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr2);
  ASSERT_TRUE(optional_specifics.has_value());
  EXPECT_EQ(kSignonRealm2,
            optional_specifics->client_only_encrypted_data().signon_realm());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotGetDataForNonExistingStorageKey) {
  const std::string kPrimaryKeyStr = "1";

  absl::optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr);
  EXPECT_FALSE(optional_specifics.has_value());
}

TEST_F(PasswordSyncBridgeTest, ShouldMergeSyncRemoteAndLocalPasswords) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  // Setup the test to have Form 1 and Form 2 stored locally, and Form 2 and
  // Form 3 coming as remote changes. We will assign primary keys for Form 1 and
  // Form 2. Form 3 will arrive as remote creation, and FakeDatabase will assign
  // it primary key 1.
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const int kExpectedPrimaryKey3 = 1;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";
  const std::string kExpectedPrimaryKeyStr3 = "1";
  PasswordForm form1 = MakePasswordForm(kSignonRealm1, kPrimaryKey1);
  PasswordForm form2 = MakePasswordForm(kSignonRealm2, kPrimaryKey2);
  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  sync_pb::PasswordSpecifics specifics2 =
      CreateSpecificsWithSignonRealm(kSignonRealm2);
  sync_pb::PasswordSpecifics specifics3 =
      CreateSpecificsWithSignonRealm(kSignonRealm3);

  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::Days(1);

  form2.date_created = yesterday;
  specifics2.mutable_client_only_encrypted_data()->set_date_created(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_db()->AddLoginWithPrimaryKey(form1);
  fake_db()->AddLoginWithPrimaryKey(form2);

  // Form 1 will be added to the change processor. The local version of Form 2
  // isn't more recent than the remote version, therefore it  will be updated in
  // the password sync store using the remote version. Form 3 will be added to
  // the password store sync.

  // Interactions should happen in this order:
  //           +--> Put(1) ----------------------------------------+
  //           |                                                   |
  //           |--> UpdateStorageKey(2) ---------------------------|
  // Begin() --|                                                   |--> Commit()
  //           |--> UpdateCredentialSync(3) -----------------------|
  //           |                                                   |
  //           +--> AddCredentialSync (4) ---> UpdateStorageKey(4)-+

  testing::Sequence s1, s2, s3, s4;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction())
      .InSequence(s1, s2, s3, s4);
  EXPECT_CALL(mock_processor(),
              Put(kPrimaryKeyStr1, EntityDataHasSignonRealm(kSignonRealm1), _))
      .InSequence(s1);

  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kPrimaryKeyStr2, _))
      .InSequence(s2);

  EXPECT_CALL(*mock_password_store_sync(),
              UpdateCredentialSync(FormHasSignonRealm(kSignonRealm2), _))
      .InSequence(s3);

  EXPECT_CALL(*mock_password_store_sync(),
              AddCredentialSync(FormHasSignonRealm(kSignonRealm3), _))
      .InSequence(s4);
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kExpectedPrimaryKeyStr3, _))
      .InSequence(s4);

  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction())
      .InSequence(s1, s2, s3, s4);

  EXPECT_CALL(*mock_password_store_sync(),
              NotifyCredentialsChanged(UnorderedElementsAre(
                  ChangeHasPrimaryKey(kPrimaryKey2),
                  ChangeHasPrimaryKey(kExpectedPrimaryKey3))))
      .InSequence(s1, s2, s3, s4);

  // Processor shouldn't be informed about Form 2 or Form 3.
  EXPECT_CALL(mock_processor(), Put(kPrimaryKeyStr2, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(kExpectedPrimaryKeyStr3, _, _)).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics2)));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics3)));

  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsChoosingTheMoreRecent) {
  // Setup the test to have Form 1 and Form 2 stored locally and remotely. Local
  // Form 1 is more recent than the remote one. Remote Form 2 is more recent
  // than the local one. We will assign primary keys for Form 1 and Form 2 in
  // the local DB.
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::Days(1);
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::string kPrimaryKeyStr2 = "1001";

  // Local form 1 is more recent than the remote.
  PasswordForm form1 = MakePasswordForm(kSignonRealm1, kPrimaryKey1);
  form1.date_created = now;
  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  specifics1.mutable_client_only_encrypted_data()->set_date_created(
      yesterday.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Remote form 2 is more recent than the local.
  PasswordForm form2 = MakePasswordForm(kSignonRealm2, kPrimaryKey2);
  form2.date_created = yesterday;
  sync_pb::PasswordSpecifics specifics2 =
      CreateSpecificsWithSignonRealm(kSignonRealm2);
  specifics2.mutable_client_only_encrypted_data()->set_date_created(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_db()->AddLoginWithPrimaryKey(form1);
  fake_db()->AddLoginWithPrimaryKey(form2);

  // The processor should be informed about the storage keys of both passwords.
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kPrimaryKeyStr1, _));
  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, kPrimaryKeyStr2, _));

  // Since local Form 1 is more recent, it will be put() in the processor.
  EXPECT_CALL(mock_processor(),
              Put(kPrimaryKeyStr1, EntityDataHasSignonRealm(kSignonRealm1), _));

  // Since the remote Form 2 is more recent, it will be updated in the password
  // store.
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateCredentialSync(FormHasSignonRealm(kSignonRealm2), _));
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics1)));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics2)));
  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

// This tests that if reading sync metadata from the store fails,
// metadata should be deleted and Sync starts without error.
TEST_F(
    PasswordSyncBridgeTest,
    ShouldMergeSyncRemoteAndLocalPasswordsWithoutErrorWhenMetadataReadFails) {
  // Simulate a failed GetAllSyncMetadata() by returning a nullptr.
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault(testing::ReturnNull());

  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasNotInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(0))));
  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

// This tests that if reading logins from the store fails,
// ShouldMergeSync() would return an error without crashing.
TEST_F(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreReadFails) {
  // Simulate a failed ReadAllCredentials() by returning a kDbError.
  ON_CALL(*mock_password_store_sync(), ReadAllCredentials)
      .WillByDefault(testing::Return(FormRetrievalResult::kDbError));
  absl::optional<syncer::ModelError> error =
      bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_TRUE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldNotDeleteSyncMetadataWhenDoesNotExist) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata());
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  auto bridge =
      PasswordSyncBridge(mock_processor().CreateForwardingProcessor(),
                         mock_password_store_sync(), base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.SyncMetadataReadError2",
                                      /*kNone*/ 0, 1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(PasswordSyncBridgeTest, ShouldRemoveSyncMetadataWhenReadAllLoginsFails) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          features::kForceInitialSyncWhenDecryptionFails,
      },
      {});
  ON_CALL(*mock_password_store_sync(), ReadAllCredentials)
      .WillByDefault(
          testing::Return(FormRetrievalResult::kEncryptionServiceFailure));

  EXPECT_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata());
  EXPECT_CALL(*mock_password_store_sync(), ReadAllCredentials)
      .WillOnce(Return(FormRetrievalResult::kEncryptionServiceFailure));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());

  auto bridge =
      PasswordSyncBridge(mock_processor().CreateForwardingProcessor(),
                         mock_password_store_sync(), base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.SyncMetadataReadError2",
                                      3, 1);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldRemoveSyncMetadataWhenSpecificsCacheContainsSupportedFields) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      syncer::kCacheBaseEntitySpecificsInMetadata);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        // Create entity with a cached supported field.
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::PasswordSpecificsData password_data;
        password_data.set_username_value("username");
        sync_pb::EntitySpecifics entity_specifics;
        *entity_specifics.mutable_password()
             ->mutable_client_only_encrypted_data() = password_data;
        sync_pb::EntityMetadata entity_metadata;
        *entity_metadata.mutable_possibly_trimmed_base_specifics() =
            entity_specifics;
        auto metadata_ptr =
            std::make_unique<sync_pb::EntityMetadata>(entity_metadata);
        metadata_batch->AddMetadata("storage_key", std::move(metadata_ptr));
        return metadata_batch;
      });

  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasNotInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(0))));

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.SyncMetadataReadError2",
                                      4, 1);
}

TEST_F(
    PasswordSyncBridgeTest,
    ShouldNotRemoveSyncMetadataWhenSpecificsCacheContainsUnsupportedFieldsOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      syncer::kCacheBaseEntitySpecificsInMetadata);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        // Create entity with a cached unsupported field.
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::PasswordSpecificsData password_data;
        *password_data.mutable_unknown_fields() = "unknown";
        sync_pb::EntitySpecifics entity_specifics;
        *entity_specifics.mutable_password()
             ->mutable_client_only_encrypted_data() = password_data;
        sync_pb::EntityMetadata entity_metadata;
        *entity_metadata.mutable_possibly_trimmed_base_specifics() =
            entity_specifics;
        auto metadata_ptr =
            std::make_unique<sync_pb::EntityMetadata>(entity_metadata);
        metadata_batch->AddMetadata("storage_key", std::move(metadata_ptr));
        return metadata_batch;
      });

  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasNotInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(1))));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

TEST_F(PasswordSyncBridgeTest,
       ShouldRemoveSyncMetadataToRedownloadPasswordNotes) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        // Create entity without the flag that the password have been
        // redownloaded for notes already.
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::ModelTypeState model_type_state;
        model_type_state.set_initial_sync_state(
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
        metadata_batch->SetModelTypeState(model_type_state);
        return metadata_batch;
      });

  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasNotInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(0))));

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.SyncMetadataReadError2",
                                      5, 1);
}

TEST_F(
    PasswordSyncBridgeTest,
    ShouldNotRemoveSyncMetadataToRedownloadPasswordNotesWhenHasBeenAlreadyRedownloaded) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        // Create entity with the flag that the password have been redownloaded
        // for notes already.
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::ModelTypeState model_type_state;
        model_type_state.set_initial_sync_state(
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
        model_type_state.set_notes_enabled_before_initial_sync_for_passwords(
            true);
        metadata_batch->SetModelTypeState(model_type_state);
        return metadata_batch;
      });

  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(0))));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

TEST_F(
    PasswordSyncBridgeTest,
    ShouldNotRemoveSyncMetadataToRedownloadPasswordNotesWhenFeatureIsDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kPasswordNotesWithBackup);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::ModelTypeState model_type_state;
        model_type_state.set_initial_sync_state(
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
        metadata_batch->SetModelTypeState(model_type_state);
        return metadata_batch;
      });

  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(0))));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

TEST_F(PasswordSyncBridgeTest,
       ShouldClearNotesBeforeSyncFlagWhenFeatureIsDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kPasswordNotesWithBackup);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::ModelTypeState model_type_state;
        model_type_state.set_initial_sync_state(
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
        model_type_state.set_notes_enabled_before_initial_sync_for_passwords(
            true);
        metadata_batch->SetModelTypeState(model_type_state);
        return metadata_batch;
      });

  EXPECT_CALL(
      mock_processor(),
      ModelReadyToSync(syncer::MetadataBatchContains(
          testing::AllOf(
              testing::Property(
                  &sync_pb::ModelTypeState::initial_sync_state,
                  sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE),
              testing::Property(
                  &sync_pb::ModelTypeState::
                      notes_enabled_before_initial_sync_for_passwords,
                  false)),
          /*entities=*/testing::SizeIs(0))));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

TEST_F(PasswordSyncBridgeTest,
       ShouldNotRemoveSyncMetadataWhenSpecificsCacheIsEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      syncer::kCacheBaseEntitySpecificsInMetadata);

  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        // Create entity with empty `possibly_trimmed_base_specifics`.
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        sync_pb::EntityMetadata entity_metadata;
        auto metadata_ptr =
            std::make_unique<sync_pb::EntityMetadata>(entity_metadata);
        metadata_batch->AddMetadata("storage_key", std::move(metadata_ptr));
        return metadata_batch;
      });

  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasNotInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(1))));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  auto bridge = std::make_unique<PasswordSyncBridge>(
      mock_processor().CreateForwardingProcessor(), mock_password_store_sync(),
      base::DoNothing());
}

TEST_F(PasswordSyncBridgeTest,
       ShouldNotRemoveSyncMetadataWhenReadAllLoginsSucceeds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          features::kForceInitialSyncWhenDecryptionFails,
      },
      {});

  EXPECT_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata());
  EXPECT_CALL(*mock_password_store_sync(), ReadAllCredentials)
      .WillOnce(Return(FormRetrievalResult::kSuccess));
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata())
      .Times(0);

  PasswordSyncBridge(mock_processor().CreateForwardingProcessor(),
                     mock_password_store_sync(), base::DoNothing());
}
#endif

// This tests that if adding logins to the store fails,
// ShouldMergeSync() would return an error without crashing.
TEST_F(PasswordSyncBridgeTest,
       ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreAddFails) {
  fake_db()->SetAddLoginError(AddCredentialError::kDbError);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));

  EXPECT_CALL(*mock_password_store_sync(), RollbackTransaction());
  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_TRUE(error);
}

// This tests that if storing model type state fails,
// ShouldMergeSync() would return an error without crashing.
TEST_F(
    PasswordSyncBridgeTest,
    ShouldMergeSyncRemoteAndLocalPasswordsWithErrorWhenStoreUpdateModelTypeStateFails) {
  // Simulate failure in UpdateModelTypeState();
  ON_CALL(*mock_sync_metadata_store_sync(), UpdateModelTypeState)
      .WillByDefault(testing::Return(false));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(model_type_state);

  EXPECT_CALL(*mock_password_store_sync(), RollbackTransaction());
  absl::optional<syncer::ModelError> error =
      bridge()->MergeFullSyncData(std::move(metadata_changes), {});
  EXPECT_TRUE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldMergeAndIgnoreAndUntrackRemotePasswordWithInvalidData) {
  fake_db()->SetAddLoginError(AddCredentialError::kConstraintViolation);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealm(kSignonRealm1);

  EXPECT_CALL(mock_processor(),
              UntrackEntityForClientTagHash(
                  SpecificsToEntity(specifics).client_tag_hash));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldGetAllDataForDebuggingWithRedactedPassword) {
  const int kPrimaryKey1 = 1000;
  const int kPrimaryKey2 = 1001;
  PasswordForm form1 = MakePasswordForm(kSignonRealm1, kPrimaryKey1);
  PasswordForm form2 = MakePasswordForm(kSignonRealm2, kPrimaryKey2);

  fake_db()->AddLoginWithPrimaryKey(form1);
  fake_db()->AddLoginWithPrimaryKey(form2);

  std::unique_ptr<syncer::DataBatch> batch;

  bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
      [&](std::unique_ptr<syncer::DataBatch> in_batch) {
        batch = std::move(in_batch);
      }));

  ASSERT_THAT(batch, NotNull());
  EXPECT_TRUE(batch->HasNext());
  while (batch->HasNext()) {
    auto [key, data] = batch->Next();
    EXPECT_EQ("<redacted>", data->specifics.password()
                                .client_only_encrypted_data()
                                .password_value());
  }
}

TEST_F(PasswordSyncBridgeTest,
       ShouldCallModelReadyUponConstructionWithMetadata) {
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        sync_pb::ModelTypeState model_type_state;
        model_type_state.set_initial_sync_state(
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
        model_type_state.set_notes_enabled_before_initial_sync_for_passwords(
            base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup));
        auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
        metadata_batch->SetModelTypeState(model_type_state);
        metadata_batch->AddMetadata(
            "storage_key", std::make_unique<sync_pb::EntityMetadata>());
        return metadata_batch;
      });

  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/syncer::HasInitialSyncDone(),
                                    /*entities=*/testing::SizeIs(1))));

  PasswordSyncBridge bridge(mock_processor().CreateForwardingProcessor(),
                            mock_password_store_sync(), base::DoNothing());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests that in case ReadAllCredentials() during initial merge returns
// encryption service failure, the bridge would try to do a DB clean up.
class PasswordSyncBridgeMergeTest
    : public PasswordSyncBridgeTest,
      public testing::WithParamInterface<FormRetrievalResult> {
 protected:
  void ShouldDeleteUndecryptableLoginsDuringMerge() {
    ON_CALL(*mock_password_store_sync(), DeleteUndecryptableCredentials())
        .WillByDefault(Return(DatabaseCleanupResult::kSuccess));

    // We should try to read first, and simulate an encryption failure. Then,
    // cleanup the database and try to read again which should be successful
    // now.
    testing::InSequence in_sequence;
    EXPECT_CALL(*mock_password_store_sync(), ReadAllCredentials)
        .WillOnce(Return(GetParam()));
    EXPECT_CALL(*mock_password_store_sync(), DeleteUndecryptableCredentials());
    EXPECT_CALL(*mock_password_store_sync(), ReadAllCredentials)
        .WillOnce(Return(FormRetrievalResult::kSuccess));

    absl::optional<syncer::ModelError> error =
        bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(), {});
    EXPECT_FALSE(error);
  }
};

TEST_P(PasswordSyncBridgeMergeTest, ShouldFixWhenDatabaseEncryptionFails) {
  ShouldDeleteUndecryptableLoginsDuringMerge();
}

INSTANTIATE_TEST_SUITE_P(
    PasswordSyncBridgeTest,
    PasswordSyncBridgeMergeTest,
    testing::Values(
        FormRetrievalResult::kEncryptionServiceFailure,
        FormRetrievalResult::kEncryptionServiceFailureWithPartialData));
#endif

TEST_F(PasswordSyncBridgeTest,
       ShouldDeleteSyncMetadataWhenApplyDisableSyncChanges) {
  EXPECT_CALL(*mock_sync_metadata_store_sync(), DeleteAllSyncMetadata());
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotifyOnSyncEnable) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // New password data becoming available because sync was newly enabled should
  // trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run());

  syncer::EntityChangeList initial_entity_data;
  initial_entity_data.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));

  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(initial_entity_data));
  ASSERT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldNotNotifyOnSyncChange) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // New password data becoming available due to an incoming sync change should
  // *not* trigger the callback. This is mainly for performance reasons: In
  // practice, this callback will cause all PasswordFormManagers to re-query
  // from the password store, which can be expensive.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run()).Times(0);

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));

  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_changes));
  ASSERT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, ShouldNotifyOnSyncDisableIfAccountStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  // The account password store gets cleared when sync is disabled, so this
  // should trigger the callback.
  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run());

  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotifyOnSyncDisableIfProfileStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(false));

  EXPECT_CALL(*mock_sync_enabled_or_disabled_cb(), Run());

  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(PasswordSyncBridgeTest, ShouldNotifyUnsyncedCredentialsIfAccountStore) {
  base::HistogramTester histogram_tester;
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));

  const std::string kPrimaryKeyUnsyncedCredentialStr = "1000";
  const std::string kPrimaryKeySyncedCredentialStr = "1001";
  const std::string kPrimaryKeyUnsyncedDeletionStr = "1002";
  const std::string kPrimaryKeyUnsyncedBlocklistStr = "1003";
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedCredentialStr))
      .WillByDefault(Return(true));
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeySyncedCredentialStr))
      .WillByDefault(Return(false));
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedDeletionStr))
      .WillByDefault(Return(true));
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedBlocklistStr))
      .WillByDefault(Return(true));

  sync_pb::EntityMetadata is_deletion_metadata;
  is_deletion_metadata.set_is_deleted(true);
  sync_pb::EntityMetadata is_not_deletion_metadata;
  is_not_deletion_metadata.set_is_deleted(false);
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        auto batch = std::make_unique<syncer::MetadataBatch>();
        batch->AddMetadata(kPrimaryKeyUnsyncedCredentialStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        batch->AddMetadata(kPrimaryKeySyncedCredentialStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        batch->AddMetadata(
            kPrimaryKeyUnsyncedDeletionStr,
            std::make_unique<sync_pb::EntityMetadata>(is_deletion_metadata));
        batch->AddMetadata(kPrimaryKeyUnsyncedBlocklistStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        return batch;
      });

  // No form is added to the database for the unsynced deletion primary key,
  // because the deletion is supposed to have already removed such form.
  const int kPrimaryKeyUnsyncedCredential = 1000;
  const int kPrimaryKeySyncedCredential = 1001;
  const int kPrimaryKeyUnsyncedBlocklist = 1003;
  PasswordForm unsynced_credential =
      MakePasswordForm(kSignonRealm1, kPrimaryKeyUnsyncedCredential);
  unsynced_credential.in_store = PasswordForm::Store::kAccountStore;
  PasswordForm synced_credential =
      MakePasswordForm(kSignonRealm2, kPrimaryKeySyncedCredential);
  synced_credential.in_store = PasswordForm::Store::kAccountStore;
  PasswordForm unsynced_blocklist =
      MakeBlocklistedForm(kSignonRealm3, kPrimaryKeyUnsyncedBlocklist);
  unsynced_blocklist.in_store = PasswordForm::Store::kAccountStore;
  fake_db()->AddLoginWithPrimaryKey(unsynced_credential);
  fake_db()->AddLoginWithPrimaryKey(synced_credential);
  fake_db()->AddLoginWithPrimaryKey(unsynced_blocklist);

  // The notification should only contain new credentials that are unsynced,
  // ignoring both synced ones, deletion entries and blocklists.
  EXPECT_CALL(*mock_password_store_sync(),
              NotifyUnsyncedCredentialsWillBeDeleted(
                  UnorderedElementsAre(unsynced_credential)));

  // The content of the metadata change list does not matter in this case.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.UnsyncedPasswordsFoundDuringSignOut", 1,
      1);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldNotNotifyUnsyncedCredentialsIfProfileStore) {
  base::HistogramTester histogram_tester;
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(false));

  const int kPrimaryKeyUnsyncedCredential = 1000;
  const std::string kPrimaryKeyUnsyncedCredentialStr = "1000";
  ON_CALL(mock_processor(), IsEntityUnsynced(kPrimaryKeyUnsyncedCredentialStr))
      .WillByDefault(Return(true));

  sync_pb::EntityMetadata is_not_deletion_metadata;
  is_not_deletion_metadata.set_is_deleted(false);
  ON_CALL(*mock_sync_metadata_store_sync(), GetAllSyncMetadata())
      .WillByDefault([&]() {
        auto batch = std::make_unique<syncer::MetadataBatch>();
        batch->AddMetadata(kPrimaryKeyUnsyncedCredentialStr,
                           std::make_unique<sync_pb::EntityMetadata>(
                               is_not_deletion_metadata));
        return batch;
      });

  PasswordForm unsynced_deletion = MakePasswordForm(kSignonRealm3);
  fake_db()->AddLoginWithPrimaryKey(
      MakePasswordForm(kSignonRealm1, kPrimaryKeyUnsyncedCredential));

  EXPECT_CALL(*mock_password_store_sync(),
              NotifyUnsyncedCredentialsWillBeDeleted)
      .Times(0);

  // The content of the metadata change list does not matter in this case.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());

  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountStorage.UnsyncedPasswordsFoundDuringSignOut", 0);
}

TEST_F(PasswordSyncBridgeTest, ShouldReportDownloadedPasswordsIfAccountStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(true));
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm1))));
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"",
      SpecificsToEntity(CreateSpecificsWithSignonRealm(kSignonRealm2))));

  sync_pb::PasswordSpecifics blocklisted_specifics;
  sync_pb::PasswordSpecificsData* password_data =
      blocklisted_specifics.mutable_client_only_encrypted_data();
  password_data->set_origin("http://www.origin.com");
  password_data->set_signon_realm(kSignonRealm3);
  password_data->set_blacklisted(true);

  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(blocklisted_specifics)));

  base::HistogramTester histogram_tester;
  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  ASSERT_FALSE(error);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreCredentialsAfterOptIn", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreBlocklistedEntriesAfterOptIn", 1, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ProfileStore.TotalAccountsBeforeInitialSync", 0);
}

TEST_F(PasswordSyncBridgeTest, ShouldReportStoredPasswordsIfProfileStore) {
  ON_CALL(*mock_password_store_sync(), IsAccountStore())
      .WillByDefault(Return(false));

  fake_db()->AddLoginWithPrimaryKey(MakePasswordForm(kSignonRealm1, 100));
  fake_db()->AddLoginWithPrimaryKey(MakePasswordForm(kSignonRealm2, 101));

  base::HistogramTester histogram_tester;
  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), syncer::EntityChangeList());
  ASSERT_FALSE(error);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsBeforeInitialSync", 2, 1);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldAddRemoteInsecureCredentilasUponRemoteCreation) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kWeak};
  const PasswordForm kForm = MakePasswordFormWithIssues(
      kSignonRealm1, /*primary_key=*/1, kIssuesTypes);
  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);

  testing::InSequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());
  EXPECT_CALL(
      *mock_password_store_sync(),
      AddCredentialSync(FormHasPasswordIssues(kForm.password_issues), _));

  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldAddRemoteInsecureCredentialsDuringInitialMerge) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const PasswordForm kForm = MakePasswordFormWithIssues(
      kSignonRealm1, /*primary_key=*/1, kIssuesTypes);
  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);

  // Form will be added to the password store sync. We use sequence since the
  // order is important. The form itself should be added before we add the
  // insecure credentials.

  testing::Sequence in_sequence;
  EXPECT_CALL(*mock_password_store_sync(), BeginTransaction());

  EXPECT_CALL(
      *mock_password_store_sync(),
      AddCredentialSync(FormHasPasswordIssues(kForm.password_issues), _));
  EXPECT_CALL(*mock_password_store_sync(), CommitTransaction());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      /*storage_key=*/"", SpecificsToEntity(specifics)));

  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_EQ(error, absl::nullopt);
}

TEST_F(PasswordSyncBridgeTest, ShouldPutSecurityIssuesOnLoginChange) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  // Since this remote creation is the first entry in the FakeDatabase, it will
  // be assigned a primary key 1.
  const int kPrimaryKey1 = 1;
  const std::string kPrimaryKeyStr1 = "1";
  std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                            InsecureType::kReused};
  PasswordForm kForm =
      MakePasswordFormWithIssues(kSignonRealm1, kPrimaryKey1, kIssuesTypes);

  fake_db()->AddLoginWithPrimaryKey(kForm);
  PasswordStoreChangeList changes;
  changes.emplace_back(PasswordStoreChange::UPDATE, kForm);
  EXPECT_CALL(
      mock_processor(),
      Put(kPrimaryKeyStr1, EntityDataHasSecurityIssueTypes(kIssuesTypes), _));

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_F(PasswordSyncBridgeTest, ShouldAddLocalSecurityIssuesDuringInitialMerge) {
  const int kPrimaryKey1 = 1000;
  const std::string kPrimaryKeyStr1 = "1000";
  std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                            InsecureType::kReused};
  const PasswordForm kForm =
      MakePasswordFormWithIssues(kSignonRealm1, kPrimaryKey1, kIssuesTypes);

  sync_pb::PasswordSpecifics specifics1 =
      CreateSpecificsWithSignonRealm(kSignonRealm1);
  fake_db()->AddLoginWithPrimaryKey(kForm);

  EXPECT_CALL(
      mock_processor(),
      Put(kPrimaryKeyStr1, EntityDataHasSecurityIssueTypes(kIssuesTypes), _));

  absl::optional<syncer::ModelError> error =
      bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(), {});
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest, GetDataWithIssuesForStorageKey) {
  const int kPrimaryKey1 = 1000;
  const std::string kPrimaryKeyStr1 = "1000";
  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};
  const PasswordForm kForm =
      MakePasswordFormWithIssues(kSignonRealm1, kPrimaryKey1, kIssuesTypes);

  fake_db()->AddLoginWithPrimaryKey(kForm);

  absl::optional<sync_pb::PasswordSpecifics> optional_specifics =
      GetDataFromBridge(/*storage_key=*/kPrimaryKeyStr1);
  ASSERT_TRUE(optional_specifics.has_value());
  ASSERT_TRUE(PasswordIssuesHasExpectedInsecureTypes(
      optional_specifics.value().client_only_encrypted_data().password_issues(),
      kIssuesTypes));
}

TEST_F(PasswordSyncBridgeTest,
       ShouldUpdateInsecureCredentialsDuringRemoteUpdate) {
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  // Add the form to the DB.
  const PasswordForm kForm = MakePasswordForm(kSignonRealm1, kPrimaryKey);
  fake_db()->AddLoginWithPrimaryKey(kForm);

  const std::vector<InsecureType> kIssuesTypes = {InsecureType::kLeaked,
                                                  InsecureType::kReused};

  // Expect that an update call will be made with the same form passed above,
  // but with added password issues.
  const PasswordForm kExpectedForm =
      MakePasswordFormWithIssues(kSignonRealm1, kPrimaryKey, kIssuesTypes);
  EXPECT_CALL(*mock_password_store_sync(),
              UpdateCredentialSync(
                  FormHasPasswordIssues(kExpectedForm.password_issues), _));

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateUpdate(
      kStorageKey, SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error =
      bridge()->ApplyIncrementalSyncChanges(
          bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldUploadLocalWhenEqualPasswordsAndLocalPhishedDuringMerge) {
  const int kPrimaryKey = 1000;
  // Test that during merge when Passwords are equal but have different
  // insecure credentials, remote data gets updated if the local password
  // has been marked as phished.
  const std::string kStorageKey = "1000";
  const std::vector<InsecureType> kLocalIssuesTypes = {InsecureType::kPhished};
  const PasswordForm kForm =
      MakePasswordFormWithIssues(kSignonRealm1, kPrimaryKey, kLocalIssuesTypes);
  std::vector<InsecureType> kRemoteIssuesTypes = {InsecureType::kReused,
                                                  InsecureType::kWeak};

  fake_db()->AddLoginWithPrimaryKey(kForm);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1,
                                              kRemoteIssuesTypes);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      kStorageKey, SpecificsToEntity(specifics)));

  EXPECT_CALL(
      mock_processor(),
      Put(kStorageKey, EntityDataHasSecurityIssueTypes(kLocalIssuesTypes), _));

  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       EqualPasswordsAndEqualInsecureCredentialsDuringMerge) {
  // Test that during merge when Passwords and insecure credentials are equal
  // there are no updates.
  const int kPrimaryKey = 1000;
  const std::string kStorageKey = "1000";
  std::vector<InsecureType> kIssuesTypes = {InsecureType::kReused,
                                            InsecureType::kWeak};
  PasswordForm kForm =
      MakePasswordFormWithIssues(kSignonRealm1, kPrimaryKey, kIssuesTypes);
  base::Time now = base::Time::Now();
  kForm.date_created = now;

  fake_db()->AddLoginWithPrimaryKey(kForm);

  sync_pb::PasswordSpecifics specifics =
      CreateSpecificsWithSignonRealmAndIssues(kSignonRealm1, kIssuesTypes);
  specifics.mutable_client_only_encrypted_data()->set_date_created(
      now.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Test that neither password store nor processor is invoked.
  EXPECT_CALL(*mock_password_store_sync(), UpdateCredentialSync).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      kStorageKey, SpecificsToEntity(specifics)));
  absl::optional<syncer::ModelError> error = bridge()->MergeFullSyncData(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error);
}

TEST_F(PasswordSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesOnlyUnknownFields) {
  sync_pb::EntitySpecifics specifics_with_only_unknown_fields;
  *specifics_with_only_unknown_fields.mutable_password()
       ->mutable_client_only_encrypted_data()
       ->mutable_unknown_fields() = "unknown_fields";

  sync_pb::EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_scheme(2);
  password_data->set_signon_realm(kSignonRealm1);
  password_data->set_origin("http://www.origin.com/");
  password_data->set_action("action");
  password_data->set_username_element("username_element");
  password_data->set_username_value("username_value");
  password_data->set_password_element("password_element");
  password_data->set_password_value("password_value");
  password_data->set_date_created(1000);
  password_data->set_blacklisted(false);
  password_data->set_type(0);
  password_data->set_times_used(1);
  password_data->set_display_name("display_name");
  password_data->set_avatar_url("avatar_url");
  password_data->set_federation_url("federation_url");
  password_data->set_date_last_used(1000);
  *password_data->mutable_password_issues() =
      CreatePasswordIssues({InsecureType::kLeaked});
  password_data->set_date_password_modified_windows_epoch_micros(1000);

  *specifics.mutable_password()
       ->mutable_client_only_encrypted_data()
       ->mutable_unknown_fields() = "unknown_fields";

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge()->TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_EQ(trimmed_specifics.SerializeAsString(),
            specifics_with_only_unknown_fields.SerializeAsString());
}

TEST_F(PasswordSyncBridgeTest,
       TrimRemoteSpecificsReturnsEmptyProtoWhenAllFieldsAreSupported) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_username_value("username_value");

  EXPECT_EQ(bridge()
                ->TrimAllSupportedFieldsFromRemoteSpecifics(specifics)
                .ByteSizeLong(),
            0u);
}

}  // namespace password_manager
