// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_MODEL_TYPE_CONTROLLER_H_

#include "components/prefs/pref_member.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_service.h"

class PrefService;

namespace password_manager {
class PasswordSenderService;

class OutgoingPasswordSharingInvitationModelTypeController
    : public syncer::ModelTypeController {
 public:
  OutgoingPasswordSharingInvitationModelTypeController(
      syncer::SyncService* sync_service,
      PasswordSenderService* password_sender_service,
      PrefService* pref_service);

  OutgoingPasswordSharingInvitationModelTypeController(
      const OutgoingPasswordSharingInvitationModelTypeController&) = delete;
  OutgoingPasswordSharingInvitationModelTypeController& operator=(
      const OutgoingPasswordSharingInvitationModelTypeController&) = delete;
  ~OutgoingPasswordSharingInvitationModelTypeController() override;

  // syncer::ModelTypeController implementation.
  PreconditionState GetPreconditionState() const override;

 private:
  void OnPasswordSharingEnabledPolicyChanged();

  const raw_ptr<syncer::SyncService> sync_service_;
  BooleanPrefMember password_sharing_enabled_policy_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_MODEL_TYPE_CONTROLLER_H_
