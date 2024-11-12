// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"

namespace {
base::Value::Dict AppPrefValue(
    std::string swb_id,
    std::string update_manifest_url,
    std::optional<web_app::UpdateChannel> update_channel,
    std::optional<base::Version> pinned_version = std::nullopt) {
  base::Value::Dict entry_dict;
  entry_dict.Set(web_app::kPolicyUpdateManifestUrlKey,
                 std::move(update_manifest_url));
  entry_dict.Set(web_app::kPolicyWebBundleIdKey, std::move(swb_id));

  if (update_channel.has_value()) {
    entry_dict.Set(web_app::kPolicyUpdateChannelKey,
                   update_channel->ToString());
  }

  if (pinned_version.has_value()) {
    entry_dict.Set(web_app::kPolicyPinnedVersionKey,
                   pinned_version->GetString());
  }

  return entry_dict;
}
}  // namespace

namespace web_app {
PolicyGenerator::PolicyGenerator() = default;
PolicyGenerator::~PolicyGenerator() = default;

void PolicyGenerator::AddForceInstalledIwa(
    web_package::SignedWebBundleId id,
    GURL update_manifest_url,
    std::optional<UpdateChannel> channel,
    std::optional<base::Version> pinned_version) {
  app_policies_.emplace_back(
      std::move(id), std::move(update_manifest_url),
      channel.has_value() ? channel.value() : UpdateChannel::default_channel(),
      pinned_version);
}

base::Value PolicyGenerator::Generate() {
  base::Value::List policy;
  for (const auto& app_policy_value : app_policies_) {
    policy.Append(AppPrefValue(
        app_policy_value.id_.id(), app_policy_value.update_manifest_url_.spec(),
        app_policy_value.update_channel_, app_policy_value.pinned_version_));
  }

  return base::Value(std::move(policy));
}

// static
base::Value PolicyGenerator::CreatePolicyEntry(
    std::string web_bundle_id,
    std::string update_manifest_url,
    std::optional<std::string> update_channel_name,
    std::optional<std::string> pinned_version) {
  auto policy_entry =
      base::Value::Dict()
          .Set(web_app::kPolicyWebBundleIdKey, web_bundle_id)
          .Set(web_app::kPolicyUpdateManifestUrlKey, update_manifest_url);

  if (update_channel_name.has_value()) {
    policy_entry.Set(web_app::kPolicyUpdateChannelKey,
                     update_channel_name.value());
  }

  if (pinned_version.has_value()) {
    policy_entry.Set(web_app::kPolicyPinnedVersionKey, pinned_version.value());
  }

  return base::Value(std::move(policy_entry));
}

PolicyGenerator::IwaForceInstalledPolicy::IwaForceInstalledPolicy(
    web_package::SignedWebBundleId id,
    GURL update_manifest_url,
    UpdateChannel channel,
    std::optional<base::Version> pinned_version)
    : id_(std::move(id)),
      update_manifest_url_(std::move(update_manifest_url)),
      update_channel_(std::move(channel)),
      pinned_version_(std::move(pinned_version)) {}
PolicyGenerator::IwaForceInstalledPolicy::IwaForceInstalledPolicy::
    IwaForceInstalledPolicy(const IwaForceInstalledPolicy& other) = default;
PolicyGenerator::IwaForceInstalledPolicy::IwaForceInstalledPolicy::
    ~IwaForceInstalledPolicy() = default;
}  // namespace web_app
