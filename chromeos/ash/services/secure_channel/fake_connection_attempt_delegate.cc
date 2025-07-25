// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_connection_attempt_delegate.h"

#include "base/check.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"

namespace ash::secure_channel {

FakeConnectionAttemptDelegate::FakeConnectionAttemptDelegate() = default;

FakeConnectionAttemptDelegate::~FakeConnectionAttemptDelegate() = default;

void FakeConnectionAttemptDelegate::OnConnectionAttemptSucceeded(
    const ConnectionDetails& connection_details,
    std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
  DCHECK(!connection_details_);
  connection_details_ = connection_details;
  authenticated_channel_ = std::move(authenticated_channel);
}

void FakeConnectionAttemptDelegate::
    OnConnectionAttemptFinishedWithoutConnection(
        const ConnectionAttemptDetails& connection_attempt_details) {
  DCHECK(!connection_attempt_details_);
  connection_attempt_details_ = connection_attempt_details;
}

}  // namespace ash::secure_channel
