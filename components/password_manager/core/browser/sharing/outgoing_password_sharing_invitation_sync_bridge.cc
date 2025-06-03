// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/sync/model/dummy_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"

namespace password_manager {

namespace {

syncer::ClientTagHash GetClientTagHashFromStorageKey(
    const std::string& storage_key) {
  return syncer::ClientTagHash::FromUnhashed(
      syncer::OUTGOING_PASSWORD_SHARING_INVITATION, storage_key);
}

std::string GetStorageKeyFromSpecifics(
    const sync_pb::OutgoingPasswordSharingInvitationSpecifics& specifics) {
  return specifics.guid();
}

sync_pb::OutgoingPasswordSharingInvitationSpecifics
CreateOutgoingPasswordSharingInvitationSpecifics(
    const PasswordForm& password_form,
    const PasswordRecipient& recipient) {
  sync_pb::OutgoingPasswordSharingInvitationSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_recipient_user_id(recipient.user_id);

  sync_pb::PasswordSharingInvitationData::PasswordData* password_data =
      specifics.mutable_client_only_unencrypted_data()->mutable_password_data();
  password_data->set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data->set_scheme(static_cast<int>(password_form.scheme));
  password_data->set_signon_realm(password_form.signon_realm);
  password_data->set_origin(
      password_form.url.is_valid() ? password_form.url.spec() : "");
  password_data->set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data->set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data->set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data->set_display_name(
      base::UTF16ToUTF8(password_form.display_name));
  password_data->set_avatar_url(
      password_form.icon_url.is_valid() ? password_form.icon_url.spec() : "");

  return specifics;
}

}  // namespace

OutgoingPasswordSharingInvitationSyncBridge::
    OutgoingPasswordSharingInvitationSyncBridge(
        std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  // Current data type doesn't have persistent storage so it's ready to sync
  // immediately.
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

OutgoingPasswordSharingInvitationSyncBridge::
    ~OutgoingPasswordSharingInvitationSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
OutgoingPasswordSharingInvitationSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The data type intentionally doesn't persist the data on disk, so metadata
  // is just ignored.
  return std::make_unique<syncer::DummyMetadataChangeList>();
}

void OutgoingPasswordSharingInvitationSyncBridge::SendPassword(
    const PasswordForm& password,
    const PasswordRecipient& recipient) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  sync_pb::OutgoingPasswordSharingInvitationSpecifics specifics =
      CreateOutgoingPasswordSharingInvitationSpecifics(password, recipient);
  const std::string storage_key = GetStorageKeyFromSpecifics(specifics);

  outgoing_invitations_in_flight_.emplace(
      GetClientTagHashFromStorageKey(storage_key),
      OutgoingInvitationWithEncryptionKey{std::move(specifics),
                                          recipient.public_key});

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(
      storage_key,
      ConvertToEntityData(
          outgoing_invitations_in_flight_[GetClientTagHashFromStorageKey(
              storage_key)]),
      metadata_change_list.get());
}

absl::optional<syncer::ModelError>
OutgoingPasswordSharingInvitationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_data.empty());
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
OutgoingPasswordSharingInvitationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    // For commit-only data type only |ACTION_DELETE| is expected.
    CHECK_EQ(syncer::EntityChange::ACTION_DELETE, change->type());

    outgoing_invitations_in_flight_.erase(
        GetClientTagHashFromStorageKey(change->storage_key()));
  }
  return absl::nullopt;
}

void OutgoingPasswordSharingInvitationSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (auto iter = outgoing_invitations_in_flight_.find(
            GetClientTagHashFromStorageKey(storage_key));
        iter != outgoing_invitations_in_flight_.end()) {
      batch->Put(storage_key, ConvertToEntityData(iter->second));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void OutgoingPasswordSharingInvitationSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [client_tag_hash, outgoing_invitation] :
       outgoing_invitations_in_flight_) {
    batch->Put(GetStorageKeyFromSpecifics(outgoing_invitation.specifics),
               ConvertToEntityData(outgoing_invitation));
  }
  std::move(callback).Run(std::move(batch));
}

std::string OutgoingPasswordSharingInvitationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string OutgoingPasswordSharingInvitationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetStorageKeyFromSpecifics(
      entity_data.specifics.outgoing_password_sharing_invitation());
}

bool OutgoingPasswordSharingInvitationSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool OutgoingPasswordSharingInvitationSyncBridge::SupportsGetStorageKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void OutgoingPasswordSharingInvitationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  outgoing_invitations_in_flight_.clear();
}

void OutgoingPasswordSharingInvitationSyncBridge::OnCommitAttemptErrors(
    const syncer::FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do not retry invalid messages and just remove them.
  for (const syncer::FailedCommitResponseData& error_response :
       error_response_list) {
    if (error_response.response_type ==
        sync_pb::CommitResponse::INVALID_MESSAGE) {
      if (error_response.datatype_specific_error
              .has_outgoing_password_sharing_invitation_error()) {
        base::UmaHistogramExactLinear(
            "Sync.OutgoingPassordSharingInvitation.CommitError",
            error_response.datatype_specific_error
                .outgoing_password_sharing_invitation_error()
                .error_code(),
            sync_pb::OutgoingPasswordSharingInvitationCommitError::
                ErrorCode_ARRAYSIZE);
      }
      change_processor()->UntrackEntityForClientTagHash(
          error_response.client_tag_hash);
      outgoing_invitations_in_flight_.erase(error_response.client_tag_hash);
    }
  }
}

syncer::ModelTypeSyncBridge::CommitAttemptFailedBehavior
OutgoingPasswordSharingInvitationSyncBridge::OnCommitAttemptFailed(
    syncer::SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (commit_error) {
    case syncer::SyncCommitError::kNetworkError:
    case syncer::SyncCommitError::kAuthError:
      // Ignore the auth error because it may be a temporary error and the
      // message will be sent on the second attempt.
      return CommitAttemptFailedBehavior::kShouldRetryOnNextCycle;
    case syncer::SyncCommitError::kServerError:
    case syncer::SyncCommitError::kBadServerResponse:
      return CommitAttemptFailedBehavior::kDontRetryOnNextCycle;
  }
}

// static
syncer::ClientTagHash OutgoingPasswordSharingInvitationSyncBridge::
    GetClientTagHashFromStorageKeyForTest(const std::string& storage_key) {
  return GetClientTagHashFromStorageKey(storage_key);
}

// static
std::unique_ptr<syncer::EntityData>
OutgoingPasswordSharingInvitationSyncBridge::ConvertToEntityData(
    const OutgoingInvitationWithEncryptionKey& invitation_with_encryption_key) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = invitation_with_encryption_key.specifics.guid();
  entity_data->client_tag_hash = GetClientTagHashFromStorageKey(
      GetStorageKeyFromSpecifics(invitation_with_encryption_key.specifics));
  entity_data->specifics.mutable_outgoing_password_sharing_invitation()
      ->CopyFrom(invitation_with_encryption_key.specifics);
  entity_data->recipient_public_key.CopyFrom(
      invitation_with_encryption_key.recipient_public_key.ToProto());
  return entity_data;
}

}  // namespace password_manager
