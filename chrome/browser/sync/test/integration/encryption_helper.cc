// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/encryption_helper.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/sync_client.h"
#include "components/sync/service/sync_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

ServerPassphraseTypeChecker::ServerPassphraseTypeChecker(
    syncer::PassphraseType expected_passphrase_type)
    : expected_passphrase_type_(expected_passphrase_type) {}

bool ServerPassphraseTypeChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with the proper passphrase type to become "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         syncer::ProtoPassphraseInt32ToEnum(
             nigori_entities[0].specifics().nigori().passphrase_type()) ==
             expected_passphrase_type_;
}

CrossUserSharingKeysChecker::CrossUserSharingKeysChecker() = default;

bool CrossUserSharingKeysChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with cross-user sharing keys to be "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         nigori_entities[0]
             .specifics()
             .nigori()
             .has_cross_user_sharing_public_key() &&
         nigori_entities[0]
             .specifics()
             .nigori()
             .cross_user_sharing_public_key()
             .has_x25519_public_key();
}

ServerNigoriKeyNameChecker::ServerNigoriKeyNameChecker(
    const std::string& expected_key_name)
    : expected_key_name_(expected_key_name) {}

bool ServerNigoriKeyNameChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  DCHECK_EQ(nigori_entities.size(), 1U);

  const std::string given_key_name =
      nigori_entities[0].specifics().nigori().encryption_keybag().key_name();

  *os << "Waiting for a Nigori node with proper key bag encryption key name ("
      << expected_key_name_ << ") to become available on the server."
      << "The server key bag encryption key name is " << given_key_name << ".";
  return given_key_name == expected_key_name_;
}

PassphraseRequiredChecker::PassphraseRequiredChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseRequiredChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking whether passhrase is required";
  return service()->IsEngineInitialized() &&
         service()->GetUserSettings()->IsPassphraseRequired();
}

PassphraseAcceptedChecker::PassphraseAcceptedChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseAcceptedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking whether passhrase is accepted";
  switch (service()->GetUserSettings()->GetPassphraseType().value_or(
      syncer::PassphraseType::kKeystorePassphrase)) {
    case syncer::PassphraseType::kKeystorePassphrase:
    case syncer::PassphraseType::kTrustedVaultPassphrase:
      return false;
    // With kImplicitPassphrase the user needs to enter the passphrase even
    // though it's not treated as an explicit passphrase.
    case syncer::PassphraseType::kImplicitPassphrase:
    case syncer::PassphraseType::kFrozenImplicitPassphrase:
    case syncer::PassphraseType::kCustomPassphrase:
      break;
  }
  return service()->IsEngineInitialized() &&
         !service()->GetUserSettings()->IsPassphraseRequired();
}

PassphraseTypeChecker::PassphraseTypeChecker(
    syncer::SyncServiceImpl* service,
    syncer::PassphraseType expected_passphrase_type)
    : SingleClientStatusChangeChecker(service),
      expected_passphrase_type_(expected_passphrase_type) {}

bool PassphraseTypeChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking expected passhrase type";
  return service()->GetUserSettings()->GetPassphraseType() ==
         expected_passphrase_type_;
}

TrustedVaultKeyRequiredStateChecker::TrustedVaultKeyRequiredStateChecker(
    syncer::SyncServiceImpl* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool TrustedVaultKeyRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault keys are " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ==
         desired_state_;
}

TrustedVaultKeysChangedStateChecker::TrustedVaultKeysChangedStateChecker(
    syncer::SyncServiceImpl* service)
    : service_(service), keys_changed_(false) {
  service->GetSyncClientForTest()->GetTrustedVaultClient()->AddObserver(this);
}

TrustedVaultKeysChangedStateChecker::~TrustedVaultKeysChangedStateChecker() {
  service_->GetSyncClientForTest()->GetTrustedVaultClient()->RemoveObserver(
      this);
}

bool TrustedVaultKeysChangedStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for trusted vault keys change";
  return keys_changed_;
}

void TrustedVaultKeysChangedStateChecker::OnTrustedVaultKeysChanged() {
  keys_changed_ = true;
  CheckExitCondition();
}

void TrustedVaultKeysChangedStateChecker::
    OnTrustedVaultRecoverabilityChanged() {}

TrustedVaultRecoverabilityDegradedStateChecker::
    TrustedVaultRecoverabilityDegradedStateChecker(
        syncer::SyncServiceImpl* service,
        bool degraded)
    : SingleClientStatusChangeChecker(service), degraded_(degraded) {}

bool TrustedVaultRecoverabilityDegradedStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault recoverability degraded state is "
      << degraded_;
  return service()->GetUserSettings()->IsTrustedVaultRecoverabilityDegraded() ==
         degraded_;
}
