// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/supervised_user/child_accounts/child_account_service_android.h"
#endif

// static
std::unique_ptr<SupervisedUserGoogleAuthNavigationThrottle>
SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(
    content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  if (!profile->IsChild())
    return nullptr;

  return base::WrapUnique(new SupervisedUserGoogleAuthNavigationThrottle(
      profile, navigation_handle));
}

SupervisedUserGoogleAuthNavigationThrottle::
    SupervisedUserGoogleAuthNavigationThrottle(
        Profile* profile,
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      child_account_service_(ChildAccountServiceFactory::GetForProfile(profile))
#if BUILDFLAG(IS_ANDROID)
      ,
      has_shown_reauth_(false)
#endif
{
}

SupervisedUserGoogleAuthNavigationThrottle::
    ~SupervisedUserGoogleAuthNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

const char* SupervisedUserGoogleAuthNavigationThrottle::GetNameForLogging() {
  return "SupervisedUserGoogleAuthNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::WillStartOrRedirectRequest() {
  // We do not yet support prerendering for supervised users.
  if (navigation_handle()->IsInPrerenderedMainFrame()) {
    return content::NavigationThrottle::CANCEL;
  }

  const GURL& url = navigation_handle()->GetURL();
  if (!google_util::IsGoogleSearchUrl(url) &&
      !google_util::IsGoogleHomePageUrl(url) &&
      !google_util::IsYoutubeDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS)) {
    return content::NavigationThrottle::PROCEED;
  }

  content::NavigationThrottle::ThrottleCheckResult result = ShouldProceed();

  if (result.action() == content::NavigationThrottle::DEFER) {
    google_auth_state_subscription_ =
        child_account_service_->ObserveGoogleAuthState(
            base::BindRepeating(&SupervisedUserGoogleAuthNavigationThrottle::
                                    OnGoogleAuthStateChanged,
                                base::Unretained(this)));
  }

  return result;
}

void SupervisedUserGoogleAuthNavigationThrottle::OnGoogleAuthStateChanged() {
  content::NavigationThrottle::ThrottleCheckResult result = ShouldProceed();

  switch (result.action()) {
    case content::NavigationThrottle::PROCEED: {
      google_auth_state_subscription_ = {};
      Resume();
      break;
    }
    case content::NavigationThrottle::CANCEL:
    case content::NavigationThrottle::CANCEL_AND_IGNORE: {
      CancelDeferredNavigation(result);
      break;
    }
    case content::NavigationThrottle::DEFER: {
      // Keep blocking.
      break;
    }
    case content::NavigationThrottle::BLOCK_REQUEST:
    case content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
    case content::NavigationThrottle::BLOCK_RESPONSE: {
      NOTREACHED();
    }
  }
}

content::NavigationThrottle::ThrottleCheckResult
SupervisedUserGoogleAuthNavigationThrottle::ShouldProceed() {
  ChildAccountService::AuthState authStatus =
      child_account_service_->GetGoogleAuthState();
  if (authStatus == ChildAccountService::AuthState::AUTHENTICATED)
    return content::NavigationThrottle::PROCEED;
  if (authStatus == ChildAccountService::AuthState::PENDING)
    return content::NavigationThrottle::DEFER;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // A credentials re-mint is already underway when we reach here (Mirror
  // account reconciliation). Nothing to do here except block the navigation
  // while re-minting is underway.
  return content::NavigationThrottle::DEFER;
#elif BUILDFLAG(IS_ANDROID)
  if (!has_shown_reauth_) {
    has_shown_reauth_ = true;

    content::WebContents* web_contents = navigation_handle()->GetWebContents();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    // This class doesn't care about browser sync consent.
    CoreAccountInfo account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (account_info.IsEmpty()) {
      // No primary account (can happen when it was removed from the device).
      return content::NavigationThrottle::DEFER;
    }

    if (skip_jni_call_for_testing_) {
      // Returns callback without JNI call for testing. Resets
      // has_shown_reauth_.
      base::BindRepeating(
          &SupervisedUserGoogleAuthNavigationThrottle::OnReauthenticationFailed,
          weak_ptr_factory_.GetWeakPtr())
          .Run();
    } else {
      ReauthenticateChildAccount(
          web_contents, account_info.email,
          base::BindRepeating(&SupervisedUserGoogleAuthNavigationThrottle::
                                  OnReauthenticationFailed,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
  return content::NavigationThrottle::DEFER;
#else
  // On other platforms we do not currently provide the same guarantees that a
  // user must be signed in for relevant domains.
  // Allow the navigation to proceed even in an unauthenticated state.
  //
  // TODO(b/274402198): if we implement similar guarantees on Linux/Mac/Windows,
  // trigger re-auth and defer navigation.
  return content::NavigationThrottle::PROCEED;
#endif
}

void SupervisedUserGoogleAuthNavigationThrottle::OnReauthenticationFailed() {
  // Cancel the navigation if reauthentication failed.
  CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
}
