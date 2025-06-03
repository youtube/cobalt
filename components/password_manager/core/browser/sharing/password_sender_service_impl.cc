// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_sender_service_impl.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

PasswordSenderServiceImpl::PasswordSenderServiceImpl(
    std::unique_ptr<OutgoingPasswordSharingInvitationSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  CHECK(sync_bridge_);
}

PasswordSenderServiceImpl::~PasswordSenderServiceImpl() = default;

void PasswordSenderServiceImpl::SendPasswords(
    const std::vector<PasswordForm>& passwords,
    const PasswordRecipient& recipient) {
  for (const PasswordForm& password : passwords) {
    sync_bridge_->SendPassword(password, recipient);
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordSenderServiceImpl::GetControllerDelegate() {
  return sync_bridge_->change_processor()->GetControllerDelegate();
}

void PasswordSenderServiceImpl::Shutdown() {}

}  // namespace password_manager
