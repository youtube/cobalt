// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace device_sync {

class CryptAuthKey;

// The lone method ComputeKeyProof() takes a CryptAuthKey, a payload, and salt
// and returns the key proof as a string. The key proofs are used by CryptAuth
// to verify that the client has the appropriate key material.
//
// The CryptAuth v2 Enrollment protocol requires the following key proofs:
//
//   Symmetric keys: The HMAC-SHA256 of the payload, using a key derived from
//                   the input symmetric key. Schematically,
//
//       HMAC(HKDF(|symmetric_key|, |salt|, |info|), |payload|)
//
//   Asymmetric keys: A DER-encoded ECDSA signature (RFC 3279) of the
//                    concatenation of the salt and payload strings.
//                    Schematically,
//
//       Sign(|private_key|, |salt| + |payload|)
//
// Specifically, the CryptAuth v2 Enrollment protocol states that
//   1) |payload| = SyncKeysResponse::random_session_id,
//   2)    |salt| = "CryptAuth Key Proof",
//   3)    |info| = key bundle name (needed for symmetric keys only).
// In the future, key crossproofs might be employed, where the payload will
// consist of other key proofs.
//
// Requirements:
//   - Currently, the only supported key types are RAW128 and RAW256 for
//     symmetric keys and P256 for asymmetric keys.
class CryptAuthKeyProofComputer {
 public:
  CryptAuthKeyProofComputer() = default;

  CryptAuthKeyProofComputer(const CryptAuthKeyProofComputer&) = delete;
  CryptAuthKeyProofComputer& operator=(const CryptAuthKeyProofComputer&) =
      delete;

  virtual ~CryptAuthKeyProofComputer() = default;

  // Returns null if key proof computation failed.
  // Note: The parameter |info| must be non-null for symmetric keys, but it is
  // not used for asymmetric keys.
  virtual absl::optional<std::string> ComputeKeyProof(
      const CryptAuthKey& key,
      const std::string& payload,
      const std::string& salt,
      const absl::optional<std::string>& info) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
