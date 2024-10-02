// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace first_party_sets {

ScopedMockFirstPartySetsHandler::ScopedMockFirstPartySetsHandler()
    : previous_(content::FirstPartySetsHandler::GetInstance()) {
  content::FirstPartySetsHandler::SetInstanceForTesting(this);
}

ScopedMockFirstPartySetsHandler::~ScopedMockFirstPartySetsHandler() {
  content::FirstPartySetsHandler::SetInstanceForTesting(previous_);
}

bool ScopedMockFirstPartySetsHandler::IsEnabled() const {
  return true;
}

void ScopedMockFirstPartySetsHandler::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {}

absl::optional<net::FirstPartySetEntry>
ScopedMockFirstPartySetsHandler::FindEntry(
    const net::SchemefulSite& site,
    const net::FirstPartySetsContextConfig& config) const {
  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return absl::nullopt;
  }
  return global_sets_.FindEntry(site, config);
}

void ScopedMockFirstPartySetsHandler::GetContextConfigForPolicy(
    const base::Value::Dict* policy,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), config_.Clone()));
    return;
  }
  std::move(callback).Run(config_.Clone());
}

void ScopedMockFirstPartySetsHandler::ClearSiteDataOnChangedSetsForContext(
    base::RepeatingCallback<content::BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback) {
  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), config_.Clone(),
                                  cache_filter_.Clone()));
    return;
  }
  std::move(callback).Run(config_.Clone(), cache_filter_.Clone());
}

void ScopedMockFirstPartySetsHandler::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const net::FirstPartySetsContextConfig& config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  net::FirstPartySetMetadata metadata =
      global_sets_.ComputeMetadata(site, top_frame_site, party_context, config);
  if (invoke_callbacks_asynchronously_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(metadata)));
    return;
  }
  return std::move(callback).Run(std::move(metadata));
}

void ScopedMockFirstPartySetsHandler::SetContextConfig(
    net::FirstPartySetsContextConfig config) {
  config_ = std::move(config);
}

void ScopedMockFirstPartySetsHandler::SetCacheFilter(
    net::FirstPartySetsCacheFilter cache_filter) {
  cache_filter_ = std::move(cache_filter);
}

void ScopedMockFirstPartySetsHandler::SetGlobalSets(
    net::GlobalFirstPartySets global_sets) {
  global_sets_ = std::move(global_sets);
}

}  // namespace first_party_sets
