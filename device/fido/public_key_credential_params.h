// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/component_export.h"
#include "base/numerics/safe_conversions.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// Data structure containing public key credential type(string) and
// cryptographic algorithm(integer) as specified by the CTAP spec. Used as a
// request parameter for AuthenticatorMakeCredential.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialParams {
 public:
  struct COMPONENT_EXPORT(DEVICE_FIDO) CredentialInfo {
    bool operator==(const CredentialInfo& other) const;

    CredentialType type = CredentialType::kPublicKey;
    int32_t algorithm =
        base::strict_cast<int32_t>(CoseAlgorithmIdentifier::kEs256);
  };

  static absl::optional<PublicKeyCredentialParams> CreateFromCBORValue(
      const cbor::Value& cbor_value);

  explicit PublicKeyCredentialParams(
      std::vector<CredentialInfo> credential_params);
  PublicKeyCredentialParams(const PublicKeyCredentialParams& other);
  PublicKeyCredentialParams(PublicKeyCredentialParams&& other);
  PublicKeyCredentialParams& operator=(const PublicKeyCredentialParams& other);
  PublicKeyCredentialParams& operator=(PublicKeyCredentialParams&& other);
  ~PublicKeyCredentialParams();

  const std::vector<CredentialInfo>& public_key_credential_params() const {
    return public_key_credential_params_;
  }

 private:
  std::vector<CredentialInfo> public_key_credential_params_;
};

cbor::Value AsCBOR(const PublicKeyCredentialParams&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
