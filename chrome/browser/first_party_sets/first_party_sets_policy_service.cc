// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace first_party_sets {

namespace {

network::mojom::FirstPartySetsReadyEventPtr MakeReadyEvent(
    net::FirstPartySetsContextConfig config,
    net::FirstPartySetsCacheFilter cache_filter) {
  auto ready_event = network::mojom::FirstPartySetsReadyEvent::New();
  ready_event->config = std::move(config);
  ready_event->cache_filter = std::move(cache_filter);
  return ready_event;
}

const base::Value::Dict* GetOverridesPolicyForProfile(
    const PrefService* prefs) {
  return prefs ? &prefs->GetDict(first_party_sets::kFirstPartySetsOverrides)
               : nullptr;
}

bool GetEnabledPolicyForProfile(const PrefService* prefs) {
  return prefs &&
         prefs->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled);
}

}  // namespace

FirstPartySetsPolicyService::FirstPartySetsPolicyService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context);
  Init();
}

FirstPartySetsPolicyService::~FirstPartySetsPolicyService() = default;

void FirstPartySetsPolicyService::InitForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Init();
}

void FirstPartySetsPolicyService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  feature_enabled_ = base::FeatureList::IsEnabled(features::kFirstPartySets);

  if (!feature_enabled_) {
    pref_enabled_ = false;
    OnReadyToNotifyDelegates(net::FirstPartySetsContextConfig(),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  // profile is guaranteed to be non-null since we create this service with a
  // non-null `context`.
  DCHECK(profile);

  PrefService* prefs = profile->GetPrefs();
  pref_enabled_ = GetEnabledPolicyForProfile(prefs);

  // If `profile` is a system profile or a guest profile, use an empty config
  // and cache filter.
  if (profile->IsSystemProfile() || profile->IsGuestSession()) {
    OnReadyToNotifyDelegates(net::FirstPartySetsContextConfig(),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  // Immediately retrieve the associated FirstPartySetsContextConfig. We can do
  // this since the value of the FirstPartySets Overrides policy doesn't
  // dynamically refresh, and all the delegates for `context` will have the same
  // policy and thus the same config.
  content::FirstPartySetsHandler::GetInstance()->GetContextConfigForPolicy(
      GetOverridesPolicyForProfile(prefs),
      base::BindOnce(&FirstPartySetsPolicyService::OnProfileConfigReady,
                     weak_factory_.GetWeakPtr(),
                     // We should only clear site data if First-Party Sets is
                     // enabled when the service is created, to allow users
                     // to play with the FPS enabled setting without
                     // affecting user experience during the browser session.
                     pref_enabled_));
}

void FirstPartySetsPolicyService::ComputeFirstPartySetMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled()) {
    std::move(callback).Run({});
    return;
  }

  if (!config_.has_value()) {
    on_ready_callbacks_.push_back(base::BindOnce(
        &FirstPartySetsPolicyService::ComputeFirstPartySetMetadataInternal,
        weak_factory_.GetWeakPtr(), site, base::OptionalFromPtr(top_frame_site),
        party_context, std::move(callback)));
    return;
  }

  content::FirstPartySetsHandler::GetInstance()->ComputeFirstPartySetMetadata(
      site, top_frame_site, party_context, *config_, std::move(callback));
}

void FirstPartySetsPolicyService::ComputeFirstPartySetMetadataInternal(
    const net::SchemefulSite& site,
    const absl::optional<net::SchemefulSite>& top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config_.has_value());

  if (!is_enabled()) {
    std::move(callback).Run({});
    return;
  }

  content::FirstPartySetsHandler::GetInstance()->ComputeFirstPartySetMetadata(
      site, base::OptionalToPtr(top_frame_site), party_context, *config_,
      std::move(callback));
}

void FirstPartySetsPolicyService::AddRemoteAccessDelegate(
    mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
        access_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegate->SetEnabled(pref_enabled_);
  if (config_.has_value() && cache_filter_.has_value()) {
    // Since the list of First-Party Sets is static after initialization and
    // the FirstPartySetsOverrides policy doesn't support dynamic refresh, a
    // profile's `config_` is static as well.
    access_delegate->NotifyReady(
        MakeReadyEvent(config_->Clone(), cache_filter_->Clone()));
  }
  access_delegates_.Add(std::move(access_delegate));
}

void FirstPartySetsPolicyService::OnFirstPartySetsEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1366846) Add metrics here to track whether the pref is ever
  // enabled before the config is ready to be to be sent to the delegates.
  pref_enabled_ = enabled;
  for (auto& delegate : access_delegates_) {
    delegate->SetEnabled(pref_enabled_);
  }
}

void FirstPartySetsPolicyService::RegisterThrottleResumeCallback(
    base::OnceClosure resume_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_ready());
  DCHECK(is_enabled());
  on_ready_callbacks_.push_back(std::move(resume_callback));
}

void FirstPartySetsPolicyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_delegates_.Clear();
  on_ready_callbacks_.clear();
  browser_context_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
}

void FirstPartySetsPolicyService::WaitForFirstInitCompleteForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_first_init_complete_for_testing_.has_value());
  if (first_initialization_complete_for_testing_) {
    DCHECK(config_.has_value());
    std::move(callback).Run();
    return;
  }
  on_first_init_complete_for_testing_ = std::move(callback);
}

void FirstPartySetsPolicyService::OnProfileConfigReady(
    bool initially_enabled,
    net::FirstPartySetsContextConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!initially_enabled) {
    OnReadyToNotifyDelegates(std::move(config),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile->IsRegularProfile() || profile->IsGuestSession()) {
    // TODO(https://crbug.com/1348572): regular profiles and guest sessions
    // aren't mutually exclusive on ChromeOS.
    OnReadyToNotifyDelegates(std::move(config),
                             net::FirstPartySetsCacheFilter());
    return;
  }

  // Representation of the current profile to be persisted on disk.
  const std::string browser_context_id = profile->GetBaseName().AsUTF8Unsafe();

  base::RepeatingCallback<content::BrowserContext*()> browser_context_getter =
      base::BindRepeating(
          [](base::WeakPtr<FirstPartySetsPolicyService> weak_ptr) {
            return weak_ptr ? weak_ptr->browser_context() : nullptr;
          },
          weak_factory_.GetWeakPtr());

  content::FirstPartySetsHandler::GetInstance()
      ->ClearSiteDataOnChangedSetsForContext(
          browser_context_getter, browser_context_id, std::move(config),
          base::BindOnce(&FirstPartySetsPolicyService::OnReadyToNotifyDelegates,
                         weak_factory_.GetWeakPtr()));
}

absl::optional<net::FirstPartySetEntry> FirstPartySetsPolicyService::FindEntry(
    const net::SchemefulSite& site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!config_.has_value()) {
    // Track this to measure how often the First-Party Sets in the browser
    // process are queried before they are ready to answer queries.
    num_queries_before_sets_ready_++;
    return absl::nullopt;
  }

  if (!is_enabled())
    return absl::nullopt;

  return content::FirstPartySetsHandler::GetInstance()->FindEntry(
      site, config_.value());
}

bool FirstPartySetsPolicyService::IsSiteInManagedSet(
    const net::SchemefulSite& site) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!config_.has_value() || !is_enabled())
    return false;

  absl::optional<net::FirstPartySetEntryOverride> maybe_override =
      config_->FindOverride(site);
  return maybe_override.has_value() && !maybe_override->IsDeletion();
}

void FirstPartySetsPolicyService::OnReadyToNotifyDelegates(
    net::FirstPartySetsContextConfig config,
    net::FirstPartySetsCacheFilter cache_filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_ = std::move(config);
  cache_filter_ = std::move(cache_filter);
  first_initialization_complete_for_testing_ = true;
  base::UmaHistogramCounts100(
      "Cookie.FirstPartySets.NumBrowserQueriesBeforeInitialization",
      num_queries_before_sets_ready_);
  for (auto& delegate : access_delegates_) {
    delegate->NotifyReady(
        MakeReadyEvent(config_.value().Clone(), cache_filter_.value().Clone()));
  }

  base::circular_deque<base::OnceClosure> callback_queue;
  callback_queue.swap(on_ready_callbacks_);
  while (!callback_queue.empty()) {
    base::OnceClosure callback = std::move(callback_queue.front());
    callback_queue.pop_front();
    std::move(callback).Run();
  }

  if (on_first_init_complete_for_testing_.has_value()) {
    std::move(on_first_init_complete_for_testing_).value().Run();
  }
}

void FirstPartySetsPolicyService::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  feature_enabled_ = true;
  pref_enabled_ = true;
  access_delegates_.Clear();
  on_ready_callbacks_.clear();
  config_.reset();
  cache_filter_.reset();
  on_first_init_complete_for_testing_.reset();
  // Note: `first_initialization_complete_for_testing_` is intentionally not
  // reset here.
  num_queries_before_sets_ready_ = 0;
}

}  // namespace first_party_sets
