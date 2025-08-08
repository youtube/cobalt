// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/suitable_origin.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "components/attribution_reporting/is_origin_suitable.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {

bool IsSitePotentiallySuitable(const net::SchemefulSite& site) {
  return site.has_registrable_domain_or_host() &&
         site.GetURL().SchemeIsHTTPOrHTTPS();
}

// static
bool SuitableOrigin::IsSuitable(const url::Origin& origin) {
  return IsOriginSuitable(origin);
}

// static
absl::optional<SuitableOrigin> SuitableOrigin::Create(url::Origin origin) {
  if (!IsSuitable(origin))
    return absl::nullopt;

  return SuitableOrigin(std::move(origin));
}

// static
absl::optional<SuitableOrigin> SuitableOrigin::Create(const GURL& url) {
  return Create(url::Origin::Create(url));
}

// static
absl::optional<SuitableOrigin> SuitableOrigin::Deserialize(
    base::StringPiece str) {
  return Create(GURL(str));
}

SuitableOrigin::SuitableOrigin(mojo::DefaultConstruct::Tag) {
  DCHECK(!IsValid());
}

SuitableOrigin::SuitableOrigin(url::Origin origin)
    : origin_(std::move(origin)) {
  DCHECK(IsValid());
}

SuitableOrigin::~SuitableOrigin() = default;

SuitableOrigin::SuitableOrigin(const SuitableOrigin&) = default;

SuitableOrigin& SuitableOrigin::operator=(const SuitableOrigin&) = default;

SuitableOrigin::SuitableOrigin(SuitableOrigin&&) = default;

SuitableOrigin& SuitableOrigin::operator=(SuitableOrigin&&) = default;

bool SuitableOrigin::operator<(const SuitableOrigin& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  return origin_ < other.origin_;
}

std::string SuitableOrigin::Serialize() const {
  DCHECK(IsValid());
  return origin_.Serialize();
}

bool SuitableOrigin::IsValid() const {
  return IsSuitable(origin_);
}

}  // namespace attribution_reporting
