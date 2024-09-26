// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FAKE_PSM_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FAKE_PSM_DELEGATE_H_

#include "base/component_export.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY) FakePsmDelegate
    : public PsmDelegateInterface {
 public:
  FakePsmDelegate(const std::string& ec_cipher_key,
                  const std::string& seed,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>
                      plaintext_ids);
  FakePsmDelegate(const FakePsmDelegate&) = delete;
  FakePsmDelegate& operator=(const FakePsmDelegate&) = delete;
  ~FakePsmDelegate() override;

  // PsmDelegateInterface:
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(private_membership::rlwe::RlweUseCase use_case,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>&
                      plaintext_ids) override;

 private:
  // Used by the PSM client to generate deterministic request/response protos.
  std::string ec_cipher_key_;
  std::string seed_;
  std::vector<private_membership::rlwe::RlwePlaintextId> plaintext_ids_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FAKE_PSM_DELEGATE_H_
