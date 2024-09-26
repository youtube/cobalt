// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class Time;
}

namespace signin {

// Creates header and payload parts of a registration JWT.
absl::optional<std::string> CreateKeyRegistrationHeaderAndPayload(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    base::StringPiece client_id,
    base::StringPiece auth_code,
    const GURL& registration_url,
    base::Time timestamp);

// Appends `signature` to provided `header_and_payload` to form a complete JWT.
std::string AppendSignatureToHeaderAndPayload(
    base::StringPiece header_and_payload,
    base::span<const uint8_t> signature);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_UTILS_H_
