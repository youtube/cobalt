// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_launch_web_auth_flow_function.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

IdentityLaunchWebAuthFlowFunction::Error WebAuthFlowFailureToError(
    WebAuthFlow::Failure failure) {
  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
      return IdentityLaunchWebAuthFlowFunction::Error::kUserRejected;
    case WebAuthFlow::INTERACTION_REQUIRED:
      return IdentityLaunchWebAuthFlowFunction::Error::kInteractionRequired;
    case WebAuthFlow::LOAD_FAILED:
      return IdentityLaunchWebAuthFlowFunction::Error::kPageLoadFailure;
    case WebAuthFlow::TIMED_OUT:
      return IdentityLaunchWebAuthFlowFunction::Error::kPageLoadTimedOut;
    case WebAuthFlow::CANNOT_CREATE_WINDOW:
      return IdentityLaunchWebAuthFlowFunction::Error::kCannotCreateWindow;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      return IdentityLaunchWebAuthFlowFunction::Error::kUnexpectedError;
  }
}

std::string ErrorToString(IdentityLaunchWebAuthFlowFunction::Error error) {
  switch (error) {
    case IdentityLaunchWebAuthFlowFunction::Error::kNone:
      NOTREACHED()
          << "This function is not expected to be called with no error";
      return std::string();
    case IdentityLaunchWebAuthFlowFunction::Error::kOffTheRecord:
      return identity_constants::kOffTheRecord;
    case IdentityLaunchWebAuthFlowFunction::Error::kUserRejected:
      return identity_constants::kUserRejected;
    case IdentityLaunchWebAuthFlowFunction::Error::kInteractionRequired:
      return identity_constants::kInteractionRequired;
    case IdentityLaunchWebAuthFlowFunction::Error::kPageLoadFailure:
      return identity_constants::kPageLoadFailure;
    case IdentityLaunchWebAuthFlowFunction::Error::kUnexpectedError:
      return identity_constants::kInvalidRedirect;
    case IdentityLaunchWebAuthFlowFunction::Error::kPageLoadTimedOut:
      return identity_constants::kPageLoadTimedOut;
    case IdentityLaunchWebAuthFlowFunction::Error::kCannotCreateWindow:
      return identity_constants::kCannotCreateWindow;
    case IdentityLaunchWebAuthFlowFunction::Error::kInvalidURLScheme:
      return identity_constants::kInvalidURLScheme;
  }
}

void RecordHistogramFunctionResult(
    IdentityLaunchWebAuthFlowFunction::Error error) {
  base::UmaHistogramEnumeration("Signin.Extensions.LaunchWebAuthFlowResult",
                                error);
}

}  // namespace

BASE_FEATURE(kNonInteractiveTimeoutForWebAuthFlow,
             "NonInteractiveTimeoutForWebAuthFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() =
    default;

IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
}

ExtensionFunction::ResponseAction IdentityLaunchWebAuthFlowFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord()) {
    Error error = Error::kOffTheRecord;

    RecordHistogramFunctionResult(error);
    return RespondNow(ExtensionFunction::Error(ErrorToString(error)));
  }

  absl::optional<api::identity::LaunchWebAuthFlow::Params> params =
      api::identity::LaunchWebAuthFlow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL auth_url(params->details.url);
  if (!auth_url.SchemeIsHTTPOrHTTPS()) {
    Error error = Error::kInvalidURLScheme;

    RecordHistogramFunctionResult(error);
    return RespondNow(ExtensionFunction::Error(ErrorToString(error)));
  }

  WebAuthFlow::Mode mode =
      params->details.interactive && *params->details.interactive
          ? WebAuthFlow::INTERACTIVE
          : WebAuthFlow::SILENT;

  auto abort_on_load_for_non_interactive = WebAuthFlow::AbortOnLoad::kYes;
  absl::optional<base::TimeDelta> timeout_for_non_interactive = absl::nullopt;
  if (base::FeatureList::IsEnabled(kNonInteractiveTimeoutForWebAuthFlow)) {
    abort_on_load_for_non_interactive =
        params->details.abort_on_load_for_non_interactive.value_or(true)
            ? WebAuthFlow::AbortOnLoad::kYes
            : WebAuthFlow::AbortOnLoad::kNo;
    if (params->details.timeout_ms_for_non_interactive) {
      timeout_for_non_interactive = std::clamp(
          base::Milliseconds(*params->details.timeout_ms_for_non_interactive),
          base::TimeDelta(), WebAuthFlow::kNonInteractiveMaxTimeout);
    }
  }

  // Set up acceptable target URLs. (Does not include chrome-extension
  // scheme for this version of the API.)
  InitFinalRedirectURLDomains(
      extension()->id(),
      Profile::FromBrowserContext(browser_context())
          ->GetPrefs()
          ->GetDict(extensions::pref_names::kOAuthRedirectUrls)
          .FindList(extension()->id()));

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.

  auth_flow_ = std::make_unique<WebAuthFlow>(
      this, profile, auth_url, mode, user_gesture(),
      abort_on_load_for_non_interactive, timeout_for_non_interactive);
  // An extension might call `launchWebAuthFlow()` with any URL. Add an infobar
  // to attribute displayed URL to the extension.
  auth_flow_->SetShouldShowInfoBar(extension()->name());

  auth_flow_->Start();
  return RespondLater();
}

bool IdentityLaunchWebAuthFlowFunction::ShouldKeepWorkerAliveIndefinitely() {
  // `identity.launchWebAuthFlow()` can trigger an interactive signin flow for
  // the user, and should thus keep the extension alive indefinitely.
  return true;
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLDomainsForTest(
    const std::string& extension_id) {
  InitFinalRedirectURLDomains(extension_id, nullptr);
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLDomains(
    const std::string& extension_id,
    const base::Value::List* redirect_urls) {
  if (!final_url_domains_.empty()) {
    return;
  }
  final_url_domains_.emplace_back(base::StringPrintf(
      kChromiumDomainRedirectUrlPattern, extension_id.c_str()));
  if (redirect_urls) {
    for (const auto& value : *redirect_urls) {
      GURL domain(value.GetString());
      if (domain.is_valid()) {
        final_url_domains_.push_back(domain.Resolve("/"));
      }
    }
  }
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowFailure(
    WebAuthFlow::Failure failure) {
  Error error = WebAuthFlowFailureToError(failure);

  RecordHistogramFunctionResult(error);
  RespondWithError(ErrorToString(error));
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
  Release();  // Balanced in Run.
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowURLChange(
    const GURL& redirect_url) {
  if (!base::Contains(final_url_domains_, redirect_url.Resolve("/"))) {
    return;
  }
  RecordHistogramFunctionResult(
      IdentityLaunchWebAuthFlowFunction::Error::kNone);
  Respond(WithArguments(redirect_url.spec()));
  if (auth_flow_) {
    auth_flow_.release()->DetachDelegateAndDelete();
  }
  Release();  // Balanced in RunAsync.
}

WebAuthFlow* IdentityLaunchWebAuthFlowFunction::GetWebAuthFlowForTesting() {
  return auth_flow_.get();
}

}  // namespace extensions
