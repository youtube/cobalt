// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"

#include <optional>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#endif

namespace apps::test {
namespace {

std::optional<std::string> WaitForNextMessage(
    content::DOMMessageQueue& message_queue) {
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  if (message.empty()) {
    return std::nullopt;
  }
  std::string unquoted_message;
  EXPECT_TRUE(base::RemoveChars(message, "\"", &unquoted_message)) << message;
  return unquoted_message;
}

std::vector<base::test::FeatureRefAndParams>
GetFeaturesToEnableLinkCapturingUXHelper(const std::string& default_state) {
  return {
      {::features::kPwaNavigationCapturing,
       {{::features::kNavigationCapturingDefaultState.name, default_state}}}};
}

}  // namespace

bool ShouldLinksWithExistingFrameTargetsCapture(
    LinkCapturingFeatureVersion version) {
  switch (version) {
    case LinkCapturingFeatureVersion::kV2DefaultOff:
      return false;
    case LinkCapturingFeatureVersion::kV2DefaultOnViaClientMode:
      return false;
    case LinkCapturingFeatureVersion::kV2DefaultOn:
      return false;
  }
}

std::string ToString(LinkCapturingFeatureVersion version) {
  switch (version) {
    case LinkCapturingFeatureVersion::kV2DefaultOff:
      return "V2DefaultOff";
    case LinkCapturingFeatureVersion::kV2DefaultOnViaClientMode:
      return "V2DefaultOnViaClientMode";
    case LinkCapturingFeatureVersion::kV2DefaultOn:
      return "V2DefaultOn";
  }
}

std::string LinkCapturingVersionToString(
    const testing::TestParamInfo<LinkCapturingFeatureVersion>& version) {
  return ToString(version.param);
}

std::vector<base::test::FeatureRefAndParams> GetFeaturesToEnableLinkCapturingUX(
    LinkCapturingFeatureVersion version) {
  CHECK_IS_TEST();
  switch (version) {
    case LinkCapturingFeatureVersion::kV2DefaultOff:
      return GetFeaturesToEnableLinkCapturingUXHelper("reimpl_default_off");
    case LinkCapturingFeatureVersion::kV2DefaultOnViaClientMode:
      return GetFeaturesToEnableLinkCapturingUXHelper(
          "reimpl_on_via_client_mode");
    case LinkCapturingFeatureVersion::kV2DefaultOn:
      return GetFeaturesToEnableLinkCapturingUXHelper("reimpl_default_on");
  }
}

base::expected<void, std::string> EnableLinkCapturingByUser(
    Profile* profile,
    const webapps::AppId& app_id) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  apps_util::SetSupportedLinksPreferenceAndWait(profile, app_id);
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile);
  base::test::TestFuture<void> preference_set;
  provider->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, /*set_to_preferred=*/true, preference_set.GetCallback());
  if (!preference_set.Wait()) {
    return base::unexpected("Enable link capturing command never completed.");
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return base::ok();
}

base::expected<void, std::string> DisableLinkCapturingByUser(
    Profile* profile,
    const webapps::AppId& app_id) {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_CHROMEOS)
  apps_util::RemoveSupportedLinksPreferenceAndWait(profile, app_id);
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile);
  base::test::TestFuture<void> preference_set;
  provider->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, /*set_to_preferred=*/false, preference_set.GetCallback());
  if (!preference_set.Wait()) {
    return base::unexpected("Disable link capturing command never completed.");
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return base::ok();
}

NavigationCommittedForUrlObserver::NavigationCommittedForUrlObserver(
    const GURL& url)
    : url_(url) {
  AddAllBrowsers();
}

NavigationCommittedForUrlObserver::~NavigationCommittedForUrlObserver() =
    default;

std::unique_ptr<base::CheckedObserver>
NavigationCommittedForUrlObserver::ProcessOneContents(
    content::WebContents* web_contents) {
  return std::make_unique<content::DidFinishNavigationObserver>(
      web_contents, base::BindRepeating(
                        &NavigationCommittedForUrlObserver::DidFinishNavigation,
                        base::Unretained(this)));
}

void NavigationCommittedForUrlObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->HasCommitted()) {
    return;
  }
  if (!handle->IsInMainFrame() || handle->GetURL() != url_) {
    return;
  }
  // Record the first match.
  if (!web_contents_) {
    web_contents_ = handle->GetWebContents();
  }
  ConditionMet();
}

void FlushLaunchQueuesForAllBrowserTabs() {
  web_app::test::RunForAllTabs(
      base::BindRepeating([](content::WebContents& web_contents) {
        web_app::WebAppTabHelper* helper =
            web_app::WebAppTabHelper::FromWebContents(&web_contents);
        if (!helper) {
          return;
        }
        helper->FlushLaunchQueueForTesting();
      }));
}

base::expected<void, std::vector<std::string>>
ResolveWebContentsWaitingForLaunchQueueFlush() {
  std::vector<std::string> errors;
  web_app::test::RunForAllTabs(
      base::BindLambdaForTesting([&](content::WebContents& web_contents) {
        content::EvalJsResult has_function = content::EvalJs(
            &web_contents, "typeof resolveLaunchParamsFlush !== 'undefined'");
        if (!has_function.is_ok() || !has_function.ExtractBool()) {
          // Sometimes the web contents is destroyed while evaluating this
          // javascript. That is fine.
          DLOG_IF(INFO, !has_function.is_ok()) << "Got error: " << has_function;
          return;
        }
        content::EvalJsResult result =
            content::EvalJs(&web_contents, "resolveLaunchParamsFlush()");
        if (!result.is_ok()) {
          errors.push_back(result.ExtractError());
        }
      }));
  if (!errors.empty()) {
    return base::unexpected(std::move(errors));
  }
  return base::ok();
}

testing::AssertionResult WaitForNavigationFinishedMessage(
    content::DOMMessageQueue& message_queue) {
  // Wait for all pages to complete loading to prevent the BrowserList or tab
  // model from changing later while flushing.
  web_app::test::CompletePageLoadForAllWebContents();

  std::optional<std::string> message;
  while ((message = WaitForNextMessage(message_queue))) {
    if (!message.has_value()) {
      return testing::AssertionFailure() << "Message never received.";
    }
    DLOG(INFO) << "Got message: " << *message;
    if (base::StartsWith(*message, "FinishedNavigating")) {
      break;
    }
    if (!base::StartsWith(*message, "PleaseFlushLaunchQueue")) {
      return testing::AssertionFailure()
             << "Unrecognized message: " << *message;
    }
    // Since DOMMessageQueue doesn't helpfully tell us where the message came
    // from, we have to flush ALL queues, and resolve any promises waiting on
    // it.
    FlushLaunchQueuesForAllBrowserTabs();

    // Because some redirection and other cases can close web contents, tab
    // closures can happen during `FlushLaunchQueueForTesting`. So call the
    // resolution of the flush in a separate method.
    auto success = ResolveWebContentsWaitingForLaunchQueueFlush();
    if (success != base::ok()) {
      return testing::AssertionFailure()
             << "Errors: " << base::JoinString(success.error(), ",");
    }
  }
  if (!message.has_value()) {
    return testing::AssertionFailure() << "Message never received.";
  }
  return testing::AssertionSuccess();
}

std::vector<GURL> GetLaunchParamUrlsInContents(
    content::WebContents* contents,
    const std::string& params_variable_name) {
  std::vector<GURL> launch_params;
  content::EvalJsResult launchParamsResults =
      content::EvalJs(contents->GetPrimaryMainFrame(),
                      "'" + params_variable_name + "' in window ? " +
                          params_variable_name + " : []");
  const base::ListValue& launchParamsTargetUrls =
      launchParamsResults.ExtractList();
  if (!launchParamsTargetUrls.empty()) {
    for (const base::Value& url : launchParamsTargetUrls) {
      launch_params.emplace_back(url.GetString());
    }
  }
  return launch_params;
}

}  // namespace apps::test
