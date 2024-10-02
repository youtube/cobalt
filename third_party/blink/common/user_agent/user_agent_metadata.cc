// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#include "base/pickle.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {
constexpr uint32_t kVersion = 2u;
}  // namespace

UserAgentBrandVersion::UserAgentBrandVersion(const std::string& ua_brand,
                                             const std::string& ua_version) {
  brand = ua_brand;
  version = ua_version;
}

const std::string UserAgentMetadata::SerializeBrandVersionList(
    const blink::UserAgentBrandList& ua_brand_version_list) {
  net::structured_headers::List brand_version_header =
      net::structured_headers::List();
  for (const UserAgentBrandVersion& brand_version : ua_brand_version_list) {
    if (brand_version.version.empty()) {
      brand_version_header.push_back(
          net::structured_headers::ParameterizedMember(
              net::structured_headers::Item(brand_version.brand), {}));
    } else {
      brand_version_header.push_back(
          net::structured_headers::ParameterizedMember(
              net::structured_headers::Item(brand_version.brand),
              {std::make_pair(
                  "v", net::structured_headers::Item(brand_version.version))}));
    }
  }

  return net::structured_headers::SerializeList(brand_version_header)
      .value_or("");
}

const std::string UserAgentMetadata::SerializeBrandFullVersionList() {
  return SerializeBrandVersionList(brand_full_version_list);
}

const std::string UserAgentMetadata::SerializeBrandMajorVersionList() {
  return SerializeBrandVersionList(brand_version_list);
}

// static
absl::optional<std::string> UserAgentMetadata::Marshal(
    const absl::optional<UserAgentMetadata>& in) {
  if (!in)
    return absl::nullopt;
  base::Pickle out;
  out.WriteUInt32(kVersion);

  out.WriteUInt32(in->brand_version_list.size());
  for (const auto& brand_version : in->brand_version_list) {
    out.WriteString(brand_version.brand);
    out.WriteString(brand_version.version);
  }

  out.WriteUInt32(in->brand_full_version_list.size());
  for (const auto& brand_version : in->brand_full_version_list) {
    out.WriteString(brand_version.brand);
    out.WriteString(brand_version.version);
  }

  out.WriteString(in->full_version);
  out.WriteString(in->platform);
  out.WriteString(in->platform_version);
  out.WriteString(in->architecture);
  out.WriteString(in->model);
  out.WriteBool(in->mobile);
  out.WriteString(in->bitness);
  out.WriteBool(in->wow64);
  return std::string(reinterpret_cast<const char*>(out.data()), out.size());
}

// static
absl::optional<UserAgentMetadata> UserAgentMetadata::Demarshal(
    const absl::optional<std::string>& encoded) {
  if (!encoded)
    return absl::nullopt;

  base::Pickle pickle(encoded->data(), encoded->size());
  base::PickleIterator in(pickle);

  uint32_t version;
  UserAgentMetadata out;
  if (!in.ReadUInt32(&version) || version != kVersion)
    return absl::nullopt;

  uint32_t brand_version_size;
  if (!in.ReadUInt32(&brand_version_size))
    return absl::nullopt;
  for (uint32_t i = 0; i < brand_version_size; i++) {
    UserAgentBrandVersion brand_version;
    if (!in.ReadString(&brand_version.brand))
      return absl::nullopt;
    if (!in.ReadString(&brand_version.version))
      return absl::nullopt;
    out.brand_version_list.push_back(std::move(brand_version));
  }

  uint32_t brand_full_version_size;
  if (!in.ReadUInt32(&brand_full_version_size))
    return absl::nullopt;
  for (uint32_t i = 0; i < brand_full_version_size; i++) {
    UserAgentBrandVersion brand_version;
    if (!in.ReadString(&brand_version.brand))
      return absl::nullopt;
    if (!in.ReadString(&brand_version.version))
      return absl::nullopt;
    out.brand_full_version_list.push_back(std::move(brand_version));
  }

  if (!in.ReadString(&out.full_version))
    return absl::nullopt;
  if (!in.ReadString(&out.platform))
    return absl::nullopt;
  if (!in.ReadString(&out.platform_version))
    return absl::nullopt;
  if (!in.ReadString(&out.architecture))
    return absl::nullopt;
  if (!in.ReadString(&out.model))
    return absl::nullopt;
  if (!in.ReadBool(&out.mobile))
    return absl::nullopt;
  if (!in.ReadString(&out.bitness))
    return absl::nullopt;
  if (!in.ReadBool(&out.wow64))
    return absl::nullopt;
  return absl::make_optional(std::move(out));
}

bool UserAgentBrandVersion::operator==(const UserAgentBrandVersion& a) const {
  return a.brand == brand && a.version == version;
}

bool operator==(const UserAgentMetadata& a, const UserAgentMetadata& b) {
  return a.brand_version_list == b.brand_version_list &&
         a.brand_full_version_list == b.brand_full_version_list &&
         a.full_version == b.full_version && a.platform == b.platform &&
         a.platform_version == b.platform_version &&
         a.architecture == b.architecture && a.model == b.model &&
         a.mobile == b.mobile && a.bitness == b.bitness && a.wow64 == b.wow64;
}

// static
UserAgentOverride UserAgentOverride::UserAgentOnly(const std::string& ua) {
  UserAgentOverride result;
  result.ua_string_override = ua;

  // If ua is not empty, it's assumed the system default should be used
  if (!ua.empty() &&
      base::FeatureList::IsEnabled(features::kUACHOverrideBlank)) {
    result.ua_metadata_override = UserAgentMetadata();
  }

  return result;
}

bool operator==(const UserAgentOverride& a, const UserAgentOverride& b) {
  return a.ua_string_override == b.ua_string_override &&
         a.ua_metadata_override == b.ua_metadata_override;
}

}  // namespace blink
