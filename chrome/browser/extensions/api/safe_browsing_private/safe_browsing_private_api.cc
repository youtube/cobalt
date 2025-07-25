// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_api.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"

using safe_browsing::SafeBrowsingNavigationObserverManager;

namespace extensions {

namespace {

// The number of user gestures we trace back for the referrer chain.
const int kReferrerUserGestureLimit = 2;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SafeBrowsingPrivateGetReferrerChainFunction

SafeBrowsingPrivateGetReferrerChainFunction::
    SafeBrowsingPrivateGetReferrerChainFunction() {}

SafeBrowsingPrivateGetReferrerChainFunction::
    ~SafeBrowsingPrivateGetReferrerChainFunction() {}

ExtensionFunction::ResponseAction
SafeBrowsingPrivateGetReferrerChainFunction::Run() {
  absl::optional<api::safe_browsing_private::GetReferrerChain::Params> params =
      api::safe_browsing_private::GetReferrerChain::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* contents = nullptr;

  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(),
                                    include_incognito_information(),
                                    &contents)) {
    return RespondNow(Error(
        base::StringPrintf("Could not find tab with id %d.", params->tab_id)));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
          profile->GetPrefs(), g_browser_process->safe_browsing_service()))
    return RespondNow(NoArguments());

  SafeBrowsingNavigationObserverManager* navigation_observer_manager =
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile);

  safe_browsing::ReferrerChain referrer_chain;
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager->IdentifyReferrerChainByRenderFrameHost(
          contents->GetPrimaryMainFrame(), kReferrerUserGestureLimit,
          &referrer_chain);

  // If the referrer chain is incomplete we'll append the most recent
  // navigations to referrer chain for diagnostic purposes. This only happens if
  // the user is not in incognito mode and has opted into extended reporting or
  // Scout reporting. Otherwise, |CountOfRecentNavigationsToAppend| returns 0.
  int recent_navigations_to_collect =
      SafeBrowsingNavigationObserverManager::CountOfRecentNavigationsToAppend(
          profile, profile->GetPrefs(), result);
  if (recent_navigations_to_collect > 0) {
    navigation_observer_manager->AppendRecentNavigations(
        recent_navigations_to_collect, &referrer_chain);
  }

  std::vector<api::safe_browsing_private::ReferrerChainEntry> referrer_entries;
  referrer_entries.reserve(referrer_chain.size());
  for (const auto& entry : referrer_chain) {
    referrer_entries.emplace_back(
        safe_browsing_util::ReferrerToReferrerChainEntry(entry));
  }
  return RespondNow(ArgumentList(
      api::safe_browsing_private::GetReferrerChain::Results::Create(
          referrer_entries)));
}

}  // namespace extensions
