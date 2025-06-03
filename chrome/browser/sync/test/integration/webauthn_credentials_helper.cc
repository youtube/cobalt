// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"

#include "base/rand_util.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace webauthn_credentials_helper {

using sync_datatype_helper::test;

namespace {

constexpr char kTestRpId[] = "example.com";
constexpr char kTestUserId[] = "\x01\x02\x03";

class WebAuthnCredentialsSyncIdEqualsChecker
    : public MultiClientStatusChangeChecker {
 public:
  WebAuthnCredentialsSyncIdEqualsChecker()
      : MultiClientStatusChangeChecker(test()->GetSyncServices()) {}

  // MultiClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    for (int i = 1; i < test()->num_clients(); ++i) {
      if (GetModel(0).GetAllSyncIds() != GetModel(i).GetAllSyncIds()) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace

PasskeySyncActiveChecker::PasskeySyncActiveChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}
PasskeySyncActiveChecker::~PasskeySyncActiveChecker() = default;

bool PasskeySyncActiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  return service()->GetActiveDataTypes().Has(syncer::WEBAUTHN_CREDENTIAL);
}

LocalPasskeysChangedChecker::LocalPasskeysChangedChecker(int profile)
    : profile_(profile) {
  observation_.Observe(&GetModel(profile_));
}

LocalPasskeysChangedChecker::~LocalPasskeysChangedChecker() = default;

bool LocalPasskeysChangedChecker::IsExitConditionSatisfied(std::ostream* os) {
  return satisfied_;
}

void LocalPasskeysChangedChecker::OnPasskeysChanged() {
  satisfied_ = true;
  CheckExitCondition();
}

void LocalPasskeysChangedChecker::OnPasskeyModelShuttingDown() {
  observation_.Reset();
}

LocalPasskeysMatchChecker::LocalPasskeysMatchChecker(int profile,
                                                     Matcher matcher)
    : profile_(profile), matcher_(matcher) {
  observation_.Observe(&GetModel(profile_));
}

LocalPasskeysMatchChecker::~LocalPasskeysMatchChecker() = default;

bool LocalPasskeysMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for local passkeys to match: ";
  testing::StringMatchResultListener result_listener;
  const bool matches = testing::ExplainMatchResult(
      matcher_, GetModel(profile_).GetAllPasskeys(), &result_listener);
  *os << result_listener.str();
  return matches;
}

void LocalPasskeysMatchChecker::OnPasskeysChanged() {
  CheckExitCondition();
}

void LocalPasskeysMatchChecker::OnPasskeyModelShuttingDown() {
  observation_.Reset();
}

ServerPasskeysMatchChecker::ServerPasskeysMatchChecker(Matcher matcher)
    : matcher_(matcher) {}

ServerPasskeysMatchChecker::~ServerPasskeysMatchChecker() = default;

bool ServerPasskeysMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for server passkeys to match: ";
  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByModelType(syncer::WEBAUTHN_CREDENTIAL);
  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

MockPasskeyModelObserver::MockPasskeyModelObserver(
    webauthn::PasskeyModel* model) {
  observation_.Observe(model);
}

MockPasskeyModelObserver::~MockPasskeyModelObserver() = default;

webauthn::PasskeyModel& GetModel(int profile_idx) {
  return *PasskeyModelFactory::GetForProfile(test()->GetProfile(profile_idx));
}

bool AwaitAllModelsMatch() {
  return WebAuthnCredentialsSyncIdEqualsChecker().Wait();
}

sync_pb::WebauthnCredentialSpecifics NewPasskey() {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(16));
  specifics.set_credential_id(base::RandBytesAsString(16));
  specifics.set_rp_id(kTestRpId);
  specifics.set_user_id(kTestUserId);
  specifics.set_creation_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

}  // namespace webauthn_credentials_helper
