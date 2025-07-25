// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key.h"

#include <ostream>
#include <tuple>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/types/optional_util.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"

namespace net {

CookiePartitionKey::CookiePartitionKey() = default;

CookiePartitionKey::CookiePartitionKey(
    const SchemefulSite& site,
    absl::optional<base::UnguessableToken> nonce)
    : site_(site), nonce_(nonce) {}

CookiePartitionKey::CookiePartitionKey(const GURL& url)
    : site_(SchemefulSite(url)) {}

CookiePartitionKey::CookiePartitionKey(bool from_script)
    : from_script_(from_script) {}

CookiePartitionKey::CookiePartitionKey(const CookiePartitionKey& other) =
    default;

CookiePartitionKey::CookiePartitionKey(CookiePartitionKey&& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(
    const CookiePartitionKey& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(CookiePartitionKey&& other) =
    default;

CookiePartitionKey::~CookiePartitionKey() = default;

bool CookiePartitionKey::operator==(const CookiePartitionKey& other) const {
  return site_ == other.site_ && nonce_ == other.nonce_;
}

bool CookiePartitionKey::operator!=(const CookiePartitionKey& other) const {
  return site_ != other.site_ || nonce_ != other.nonce_;
}

bool CookiePartitionKey::operator<(const CookiePartitionKey& other) const {
  return std::tie(site_, nonce_) < std::tie(other.site_, other.nonce_);
}

// static
bool CookiePartitionKey::Serialize(const absl::optional<CookiePartitionKey>& in,
                                   std::string& out) {
  if (!in) {
    out = kEmptyCookiePartitionKey;
    return true;
  }
  if (!in->IsSerializeable()) {
    DLOG(WARNING) << "CookiePartitionKey is not serializeable";
    return false;
  }
  out = in->site_.GetURL().SchemeIsFile()
            ? in->site_.SerializeFileSiteWithHost()
            : in->site_.Serialize();
  return true;
}

// static
bool CookiePartitionKey::Deserialize(const std::string& in,
                                     absl::optional<CookiePartitionKey>& out) {
  if (in == kEmptyCookiePartitionKey) {
    out = absl::nullopt;
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies)) {
    DLOG(WARNING) << "Attempting to deserialize CookiePartitionKey when "
                     "PartitionedCookies is disabled";
    return false;
  }
  auto schemeful_site = SchemefulSite::Deserialize(in);
  // SchemfulSite is opaque if the input is invalid.
  if (schemeful_site.opaque()) {
    DLOG(WARNING) << "Cannot deserialize opaque origin to CookiePartitionKey";
    return false;
  }
  out = absl::make_optional(CookiePartitionKey(schemeful_site, absl::nullopt));
  return true;
}

absl::optional<CookiePartitionKey> CookiePartitionKey::FromNetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key) {
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies)) {
    return absl::nullopt;
  }

  const absl::optional<base::UnguessableToken>& nonce =
      network_isolation_key.GetNonce();

  // Use frame site for nonced partitions. Since the nonce is unique, this still
  // creates a unique partition key. The reason we use the frame site is to
  // align CookiePartitionKey's implementation of nonced partitions with
  // StorageKey's. See https://crbug.com/1440765.
  const absl::optional<SchemefulSite>& partition_key_site =
      nonce ? network_isolation_key.GetFrameSiteForCookiePartitionKey(
                  NetworkIsolationKey::CookiePartitionKeyPassKey())
            : network_isolation_key.GetTopFrameSite();
  if (!partition_key_site)
    return absl::nullopt;

  return net::CookiePartitionKey(*partition_key_site, nonce);
}

// static
absl::optional<net::CookiePartitionKey>
CookiePartitionKey::FromStorageKeyComponents(
    const SchemefulSite& site,
    const absl::optional<base::UnguessableToken>& nonce) {
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies)) {
    return absl::nullopt;
  }
  return CookiePartitionKey::FromWire(site, nonce);
}

bool CookiePartitionKey::IsSerializeable() const {
  if (!base::FeatureList::IsEnabled(features::kPartitionedCookies)) {
    DLOG(WARNING) << "Attempting to serialize CookiePartitionKey when "
                     "PartitionedCookies feature is disabled";
    return false;
  }
  // We should not try to serialize a partition key created by a renderer.
  DCHECK(!from_script_);
  return !site_.opaque() && !nonce_.has_value();
}

std::ostream& operator<<(std::ostream& os, const CookiePartitionKey& cpk) {
  os << cpk.site();
  if (cpk.nonce().has_value()) {
    os << ",nonced";
  }
  return os;
}

}  // namespace net
