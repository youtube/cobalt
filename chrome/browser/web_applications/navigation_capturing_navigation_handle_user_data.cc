// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"

#include "base/strings/to_string.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::Disabled() {
  return NavigationCapturingRedirectionInfo(
      /*app_id_source_browser=*/std::nullopt,
      NavigationHandlingInitialResult::kNotHandledByNavigationHandling,
      /*first_navigation_app_id=*/std::nullopt, WindowOpenDisposition::UNKNOWN);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::AuxiliaryContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, NavigationHandlingInitialResult::kAuxContext,
      /*first_navigation_app_id=*/std::nullopt, disposition);
}

// Created for user-modified or capturable navigations that don't have an
// initial controlling app of the first url.
// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::NoInitialActionRedirectionHandlingEligible(
    const std::optional<webapps::AppId>& source_browser_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id, NavigationHandlingInitialResult::kBrowserTab,
      /*first_navigation_app_id=*/std::nullopt, disposition);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::ForcedNewContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const webapps::AppId& capturing_app_id,
    blink::mojom::DisplayMode capturing_display_mode,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id,
      capturing_display_mode == blink::mojom::DisplayMode::kBrowser
          ? NavigationHandlingInitialResult::kForcedNewAppContextBrowserTab
          : NavigationHandlingInitialResult::kForcedNewAppContextAppWindow,
      capturing_app_id, disposition);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::CapturedNewContext(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const webapps::AppId& capturing_app_id,
    blink::mojom::DisplayMode capturing_display_mode,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id,
      capturing_display_mode == blink::mojom::DisplayMode::kBrowser
          ? NavigationHandlingInitialResult::kNavigateCapturedNewBrowserTab
          : NavigationHandlingInitialResult::kNavigateCapturedNewAppWindow,
      capturing_app_id, disposition);
}

// static
NavigationCapturingRedirectionInfo
NavigationCapturingRedirectionInfo::CapturedNavigateExisting(
    const std::optional<webapps::AppId>& source_browser_app_id,
    const webapps::AppId& capturing_app_id,
    WindowOpenDisposition disposition) {
  return NavigationCapturingRedirectionInfo(
      source_browser_app_id,
      NavigationHandlingInitialResult::kNavigateCapturingNavigateExisting,
      capturing_app_id, disposition);
}

NavigationCapturingRedirectionInfo::~NavigationCapturingRedirectionInfo() =
    default;
NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const NavigationCapturingRedirectionInfo& navigation_info) = default;
NavigationCapturingRedirectionInfo&
NavigationCapturingRedirectionInfo::operator=(
    const NavigationCapturingRedirectionInfo&) = default;

base::Value NavigationCapturingRedirectionInfo::ToDebugData() const {
  return base::Value(base::Value::Dict()
                         .Set("initial_nav_handling_result",
                              base::ToString(initial_nav_handling_result()))
                         .Set("app_id_source_browser",
                              app_id_source_browser().value_or("<none>"))
                         .Set("first_navigation_app_id",
                              first_navigation_app_id().value_or("<none>"))
                         .Set("disposition", base::ToString(disposition())));
}

NavigationCapturingRedirectionInfo::NavigationCapturingRedirectionInfo(
    const std::optional<webapps::AppId>& source_browser_app_id,
    NavigationHandlingInitialResult initial_nav_handling_result,
    const std::optional<webapps::AppId>& first_navigation_app_id,
    WindowOpenDisposition disposition)
    : app_id_source_browser_(source_browser_app_id),
      initial_nav_handling_result_(initial_nav_handling_result),
      first_navigation_app_id_(first_navigation_app_id),
      disposition_(disposition) {}

NavigationCapturingNavigationHandleUserData::
    ~NavigationCapturingNavigationHandleUserData() = default;

NavigationCapturingNavigationHandleUserData::
    NavigationCapturingNavigationHandleUserData(
        content::NavigationHandle& navigation_handle,
        NavigationCapturingRedirectionInfo redirection_info)
    : redirection_info_(std::move(redirection_info)) {}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    NavigationCapturingNavigationHandleUserData);

}  // namespace web_app
