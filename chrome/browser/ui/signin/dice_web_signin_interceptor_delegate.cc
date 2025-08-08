// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"

#include <memory>

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

class ForcedProfileSwitchInterceptionHandle
    : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  explicit ForcedProfileSwitchInterceptionHandle(
      base::OnceCallback<void(SigninInterceptionResult)> callback) {
    DCHECK(callback);
    cancelable_callback_.Reset(std::move(callback));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(cancelable_callback_.callback(),
                                  SigninInterceptionResult::kAccepted));
  }
  ~ForcedProfileSwitchInterceptionHandle() override = default;

 private:
  base::CancelableOnceCallback<void(SigninInterceptionResult)>
      cancelable_callback_;
};

class ForcedEnterpriseSigninInterceptionHandle
    : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  ForcedEnterpriseSigninInterceptionHandle(
      Browser* browser,
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback)
      : browser_(browser->AsWeakPtr()),
        bubble_parameters_(bubble_parameters),
        profile_creation_required_by_policy_(
            bubble_parameters.interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced),
        show_link_data_option_(bubble_parameters.show_link_data_option),
        callback_(std::move(callback)) {
    DCHECK(browser_);
    DCHECK(callback_);
    browser_->signin_view_controller()->ShowModalEnterpriseConfirmationDialog(
        bubble_parameters.intercepted_account,
        profile_creation_required_by_policy_, show_link_data_option_,
        base::BindOnce(&ForcedEnterpriseSigninInterceptionHandle::
                           OnEnterpriseInterceptionDialogClosed,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ~ForcedEnterpriseSigninInterceptionHandle() override {
    if (browser_) {
      browser_->signin_view_controller()->CloseModalSignin();
    }
    if (callback_) {
      DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
          bubble_parameters_, browser_->profile(),
          SigninInterceptionResult::kDeclined);
      std::move(callback_).Run(SigninInterceptionResult::kDeclined);
    }
  }

 private:
  void OnEnterpriseInterceptionDialogClosed(signin::SigninChoice result) {
    SigninInterceptionResult interception_result =
        SigninInterceptionResult::kDeclined;
    switch (result) {
      case signin::SIGNIN_CHOICE_NEW_PROFILE:
        interception_result = SigninInterceptionResult::kAccepted;
        break;
      case signin::SIGNIN_CHOICE_CONTINUE:
        DCHECK(!profile_creation_required_by_policy_ || show_link_data_option_);
        interception_result =
            SigninInterceptionResult::kAcceptedWithExistingProfile;
        break;
      case signin::SIGNIN_CHOICE_CANCEL:
        interception_result = SigninInterceptionResult::kDeclined;
        break;
      case signin::SIGNIN_CHOICE_SIZE:
      default:
        NOTREACHED();
        break;
    }
    DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
        bubble_parameters_, browser_->profile(), interception_result);
    std::move(callback_).Run(interception_result);
  }

  base::WeakPtr<Browser> browser_;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;
  const bool profile_creation_required_by_policy_;
  const bool show_link_data_option_;
  base::OnceCallback<void(SigninInterceptionResult)> callback_;
  base::WeakPtrFactory<ForcedEnterpriseSigninInterceptionHandle>
      weak_ptr_factory_{this};
};

}  // namespace

DiceWebSigninInterceptorDelegate::DiceWebSigninInterceptorDelegate() = default;

DiceWebSigninInterceptorDelegate::~DiceWebSigninInterceptorDelegate() = default;

bool DiceWebSigninInterceptorDelegate::IsSigninInterceptionSupported(
    const content::WebContents& web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(&web_contents);
  // The profile creation flow has no browser.
  if (!browser) {
    return false;
  }
  return IsSigninInterceptionSupportedInternal(*browser);
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubble(
    content::WebContents* web_contents,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  if (bubble_parameters.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced) {
    return std::make_unique<ForcedProfileSwitchInterceptionHandle>(
        std::move(callback));
  }

  if (!web_contents) {
    std::move(callback).Run(SigninInterceptionResult::kNotDisplayed);
    return nullptr;
  }

  if (bubble_parameters.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced ||
      bubble_parameters.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::
              kEnterpriseAcceptManagement) {
    return std::make_unique<ForcedEnterpriseSigninInterceptionHandle>(
        chrome::FindBrowserWithTab(web_contents), bubble_parameters,
        std::move(callback));
  }

  return ShowSigninInterceptionBubbleInternal(
      chrome::FindBrowserWithTab(web_contents), bubble_parameters,
      std::move(callback));
}

void DiceWebSigninInterceptorDelegate::ShowFirstRunExperienceInNewProfile(
    Browser* browser,
    const CoreAccountId& account_id,
    WebSigninInterceptor::SigninInterceptionType interception_type) {
  browser->signin_view_controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id,
      interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);
}

// static
void DiceWebSigninInterceptorDelegate::RecordInterceptionResult(
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    Profile* profile,
    SigninInterceptionResult result) {
  std::string histogram_base_name = "Signin.InterceptResult";
  switch (bubble_parameters.interception_type) {
    case WebSigninInterceptor::SigninInterceptionType::kEnterprise:
    case WebSigninInterceptor::SigninInterceptionType::
        kEnterpriseAcceptManagement:
      histogram_base_name.append(".Enterprise");
      break;
    case WebSigninInterceptor::SigninInterceptionType::kMultiUser:
      histogram_base_name.append(".MultiUser");
      break;
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced:
      histogram_base_name.append(".Switch");
      break;
    case WebSigninInterceptor::SigninInterceptionType::kChromeSignin:
      histogram_base_name.append(".ChromeSignin");
      break;
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced:
      histogram_base_name.append(".EnterpriseForced");
      break;
  }

  // Record aggregated histogram for each interception type.
  base::UmaHistogramEnumeration(histogram_base_name, result);
  // Record histogram sliced by Sync status.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string sync_suffix =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)
          ? ".Sync"
          : ".NoSync";
  base::UmaHistogramEnumeration(histogram_base_name + sync_suffix, result);
  // For Enterprise, slice per enterprise status for each account.
  if (bubble_parameters.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kEnterprise) {
    if (bubble_parameters.intercepted_account.IsManaged()) {
      std::string histogram_name = histogram_base_name + ".NewIsEnterprise";
      base::UmaHistogramEnumeration(histogram_name, result);
    }
    if (bubble_parameters.primary_account.IsManaged()) {
      std::string histogram_name = histogram_base_name + ".PrimaryIsEnterprise";
      base::UmaHistogramEnumeration(histogram_name, result);
    }
  }
}
