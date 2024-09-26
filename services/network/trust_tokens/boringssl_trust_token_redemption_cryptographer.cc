// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_redemption_cryptographer.h"

#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "services/network/trust_tokens/boringssl_trust_token_state.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

BoringsslTrustTokenRedemptionCryptographer::
    BoringsslTrustTokenRedemptionCryptographer() = default;

BoringsslTrustTokenRedemptionCryptographer::
    ~BoringsslTrustTokenRedemptionCryptographer() = default;

bool BoringsslTrustTokenRedemptionCryptographer::Initialize(
    mojom::TrustTokenProtocolVersion issuer_configured_version,
    int issuer_configured_batch_size) {
  state_ = BoringsslTrustTokenState::Create(issuer_configured_version,
                                            issuer_configured_batch_size);
  return !!state_;
}

absl::optional<std::string>
BoringsslTrustTokenRedemptionCryptographer::BeginRedemption(
    TrustToken token,
    const url::Origin& top_level_origin) {
  if (!state_) {
    return absl::nullopt;
  }

  // It's unclear if BoringSSL expects the exact same value in the client data
  // and as the |time| argument to TRUST_TOKEN_CLIENT_begin_redemption; play
  // it safe by using a single timestamp.
  base::Time redemption_timestamp = base::Time::Now();

  absl::optional<std::vector<uint8_t>> maybe_client_data =
      CanonicalizeTrustTokenClientDataForRedemption(redemption_timestamp,
                                                    top_level_origin);
  if (!maybe_client_data)
    return absl::nullopt;

  ScopedBoringsslBytes raw_redemption_request;

  bssl::UniquePtr<TRUST_TOKEN> boringssl_token(
      TRUST_TOKEN_new(base::as_bytes(base::make_span(token.body())).data(),
                      token.body().size()));
  if (!boringssl_token)
    return absl::nullopt;

  if (!TRUST_TOKEN_CLIENT_begin_redemption(
          state_->Get(), raw_redemption_request.mutable_ptr(),
          raw_redemption_request.mutable_len(), boringssl_token.get(),
          maybe_client_data->data(), maybe_client_data->size(),
          (redemption_timestamp - base::Time::UnixEpoch()).InSeconds())) {
    return absl::nullopt;
  }

  return base::Base64Encode(raw_redemption_request.as_span());
}

absl::optional<std::string>
BoringsslTrustTokenRedemptionCryptographer::ConfirmRedemption(
    base::StringPiece response_header) {
  if (!state_) {
    return absl::nullopt;
  }

  return std::string(response_header);
}

}  // namespace network
