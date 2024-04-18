// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net {

namespace {

std::string GetSiteDebugString(const absl::optional<SchemefulSite>& site) {
  return site ? site->GetDebugString() : "null";
}

}  // namespace

NetworkIsolationKey::NetworkIsolationKey(
    SerializationPasskey,
    SchemefulSite top_frame_site,
    SchemefulSite frame_site,
    bool is_cross_site,
    absl::optional<base::UnguessableToken> nonce)
    : top_frame_site_(std::move(top_frame_site)),
      frame_site_(std::move(frame_site)),
      is_cross_site_(is_cross_site),
      nonce_(std::move(nonce)) {
  CHECK_EQ(GetMode(), Mode::kCrossSiteFlagEnabled);
}

NetworkIsolationKey::NetworkIsolationKey(
    const SchemefulSite& top_frame_site,
    const SchemefulSite& frame_site,
    const absl::optional<base::UnguessableToken>& nonce)
    : NetworkIsolationKey(SchemefulSite(top_frame_site),
                          SchemefulSite(frame_site),
                          absl::optional<base::UnguessableToken>(nonce)) {}

NetworkIsolationKey::NetworkIsolationKey(
    SchemefulSite&& top_frame_site,
    SchemefulSite&& frame_site,
    absl::optional<base::UnguessableToken>&& nonce)
    : top_frame_site_(std::move(top_frame_site)),
      frame_site_(absl::make_optional(std::move(frame_site))),
      is_cross_site_((GetMode() == Mode::kCrossSiteFlagEnabled)
                         ? absl::make_optional(*top_frame_site_ != *frame_site_)
                         : absl::nullopt),
      nonce_(std::move(nonce)) {
  DCHECK(!nonce_ || !nonce_->is_empty());
}

NetworkIsolationKey::NetworkIsolationKey(const url::Origin& top_frame_origin,
                                         const url::Origin& frame_origin)
    : NetworkIsolationKey(SchemefulSite(top_frame_origin),
                          SchemefulSite(frame_origin)) {}

NetworkIsolationKey::NetworkIsolationKey() = default;

NetworkIsolationKey::NetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey::NetworkIsolationKey(
    NetworkIsolationKey&& network_isolation_key) = default;

NetworkIsolationKey::~NetworkIsolationKey() = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    const NetworkIsolationKey& network_isolation_key) = default;

NetworkIsolationKey& NetworkIsolationKey::operator=(
    NetworkIsolationKey&& network_isolation_key) = default;

NetworkIsolationKey NetworkIsolationKey::CreateTransient() {
  SchemefulSite site_with_opaque_origin;
  return NetworkIsolationKey(site_with_opaque_origin, site_with_opaque_origin);
}

NetworkIsolationKey NetworkIsolationKey::CreateWithNewFrameSite(
    const SchemefulSite& new_frame_site) const {
  if (!top_frame_site_)
    return NetworkIsolationKey();
  NetworkIsolationKey key(top_frame_site_.value(), new_frame_site);
  key.nonce_ = nonce_;
  return key;
}

absl::optional<std::string> NetworkIsolationKey::ToCacheKeyString() const {
  if (IsTransient())
    return absl::nullopt;

  std::string variable_key_piece;
  switch (GetMode()) {
    case Mode::kFrameSiteEnabled:
      variable_key_piece = frame_site_->Serialize();
      break;
    case Mode::kCrossSiteFlagEnabled:
      variable_key_piece = (*is_cross_site_ ? "_1" : "_0");
      break;
  }
  return top_frame_site_->Serialize() + " " + variable_key_piece;
}

std::string NetworkIsolationKey::ToDebugString() const {
  // The space-separated serialization of |top_frame_site_| and
  // |frame_site_|.
  std::string return_string = GetSiteDebugString(top_frame_site_);
  switch (GetMode()) {
    case Mode::kFrameSiteEnabled:
      return_string += " " + GetSiteDebugString(frame_site_);
      break;
    case Mode::kCrossSiteFlagEnabled:
      if (is_cross_site_.has_value()) {
        return_string += (*is_cross_site_ ? " cross-site" : " same-site");
      }
      break;
  }

  if (nonce_.has_value()) {
    return_string += " (with nonce " + nonce_->ToString() + ")";
  }

  return return_string;
}

bool NetworkIsolationKey::IsFullyPopulated() const {
  if (!top_frame_site_.has_value()) {
    return false;
  }
  if (GetMode() == Mode::kFrameSiteEnabled && !frame_site_.has_value()) {
    return false;
  }
  return true;
}

bool NetworkIsolationKey::IsTransient() const {
  if (!IsFullyPopulated())
    return true;
  return IsOpaque();
}

// static
NetworkIsolationKey::Mode NetworkIsolationKey::GetMode() {
  if (base::FeatureList::IsEnabled(
          net::features::kEnableCrossSiteFlagNetworkIsolationKey)) {
    return Mode::kCrossSiteFlagEnabled;
  } else {
    return Mode::kFrameSiteEnabled;
  }
}

const absl::optional<SchemefulSite>& NetworkIsolationKey::GetFrameSite() const {
  // Frame site will be empty if double-keying is enabled.
  CHECK(GetMode() == Mode::kFrameSiteEnabled);
  return frame_site_;
}

absl::optional<bool> NetworkIsolationKey::GetIsCrossSite() const {
  CHECK(GetMode() == Mode::kCrossSiteFlagEnabled);
  return is_cross_site_;
}

bool NetworkIsolationKey::IsEmpty() const {
  return !top_frame_site_.has_value() && !frame_site_.has_value();
}

bool NetworkIsolationKey::IsOpaque() const {
  if (top_frame_site_->opaque()) {
    return true;
  }
  if (GetMode() == Mode::kFrameSiteEnabled && frame_site_->opaque()) {
    return true;
  }
  if (nonce_.has_value()) {
    return true;
  }
  return false;
}

}  // namespace net
