// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/echo_private_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/echo_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace echo_api = extensions::api::echo_private;

EchoPrivateGetRegistrationCodeFunction::
    EchoPrivateGetRegistrationCodeFunction() = default;

EchoPrivateGetRegistrationCodeFunction::
    ~EchoPrivateGetRegistrationCodeFunction() = default;

ExtensionFunction::ResponseAction
EchoPrivateGetRegistrationCodeFunction::Run() {
  std::optional<echo_api::GetRegistrationCode::Params> params =
      echo_api::GetRegistrationCode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kGroupType = "GROUP_CODE";
  std::optional<crosapi::mojom::RegistrationCodeType> type;
  if (params->type == kCouponType) {
    type = crosapi::mojom::RegistrationCodeType::kCoupon;
  } else if (params->type == kGroupType) {
    type = crosapi::mojom::RegistrationCodeType::kGroup;
  }

  if (!type) {
    return RespondNow(ArgumentList(
        echo_api::GetRegistrationCode::Results::Create(std::string())));
  }

  auto callback = base::BindOnce(
      &EchoPrivateGetRegistrationCodeFunction::RespondWithResult, this);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->echo_private_ash()
      ->GetRegistrationCode(type.value(), std::move(callback));
  return RespondLater();
}

void EchoPrivateGetRegistrationCodeFunction::RespondWithResult(
    const std::string& result) {
  Respond(WithArguments(result));
}

EchoPrivateSetOfferInfoFunction::EchoPrivateSetOfferInfoFunction() = default;

EchoPrivateSetOfferInfoFunction::~EchoPrivateSetOfferInfoFunction() = default;

ExtensionFunction::ResponseAction EchoPrivateSetOfferInfoFunction::Run() {
  std::optional<echo_api::SetOfferInfo::Params> params =
      echo_api::SetOfferInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  base::Value::Dict dict = params->offer_info.additional_properties.Clone();
  chromeos::echo_offer::RemoveEmptyValueDicts(dict);

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate offer_update(local_state, prefs::kEchoCheckedOffers);
  offer_update->Set("echo." + service_id, std::move(dict));
  return RespondNow(NoArguments());
}

EchoPrivateGetOfferInfoFunction::EchoPrivateGetOfferInfoFunction() = default;

EchoPrivateGetOfferInfoFunction::~EchoPrivateGetOfferInfoFunction() = default;

ExtensionFunction::ResponseAction EchoPrivateGetOfferInfoFunction::Run() {
  std::optional<echo_api::GetOfferInfo::Params> params =
      echo_api::GetOfferInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& offer_infos =
      local_state->GetDict(prefs::kEchoCheckedOffers);

  const base::Value* offer_info = offer_infos.Find("echo." + service_id);
  if (!offer_info || !offer_info->is_dict()) {
    return RespondNow(Error("Not found"));
  }

  echo_api::GetOfferInfo::Results::Result result;
  result.additional_properties.Merge(offer_info->GetDict().Clone());
  return RespondNow(
      ArgumentList(echo_api::GetOfferInfo::Results::Create(result)));
}

EchoPrivateGetOobeTimestampFunction::EchoPrivateGetOobeTimestampFunction() =
    default;

EchoPrivateGetOobeTimestampFunction::~EchoPrivateGetOobeTimestampFunction() =
    default;

ExtensionFunction::ResponseAction EchoPrivateGetOobeTimestampFunction::Run() {
  std::optional<base::Time> timestamp =
      ash::report::utils::GetFirstActiveWeek();

  if (!timestamp.has_value()) {
    // Returns an empty string on error.
    return RespondNow(WithArguments(std::string()));
  }
  std::string result = base::UnlocalizedTimeFormatWithPattern(
      timestamp.value(), "y-M-d", icu::TimeZone::getGMT());
  return RespondNow(WithArguments(std::move(result)));
}

EchoPrivateGetUserConsentFunction::EchoPrivateGetUserConsentFunction() =
    default;

EchoPrivateGetUserConsentFunction::~EchoPrivateGetUserConsentFunction() =
    default;

ExtensionFunction::ResponseAction EchoPrivateGetUserConsentFunction::Run() {
  std::optional<echo_api::GetUserConsent::Params> params =
      echo_api::GetUserConsent::Params::Create(args());

  // Verify that the passed origin URL is valid.
  GURL service_origin = GURL(params->consent_requester.origin);
  if (!service_origin.is_valid()) {
    return RespondNow(Error("Invalid origin."));
  }

  content::WebContents* web_contents = nullptr;
  if (!params->consent_requester.tab_id) {
    web_contents = GetSenderWebContents();

    if (!web_contents || extensions::GetViewType(web_contents) !=
                             extensions::mojom::ViewType::kAppWindow) {
      return RespondNow(
          Error("Not called from an app window - the tabId is required."));
    }
  } else {
    extensions::WindowController* window = nullptr;
    int tab_index = -1;
    if (!extensions::ExtensionTabUtil::GetTabById(
            *params->consent_requester.tab_id, browser_context(),
            false /*incognito_enabled*/, &window, &web_contents, &tab_index) ||
        !window) {
      return RespondNow(Error("Tab not found."));
    }

    // Bail out if the requested tab is not active - the dialog is modal to the
    // window, so showing it for a request from an inactive tab could be
    // misleading/confusing to the user.
    if (tab_index != window->GetBrowser()->tab_strip_model()->active_index()) {
      return RespondNow(Error("Consent requested from an inactive tab."));
    }
  }

  DCHECK(web_contents);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->echo_private_ash()
      ->CheckRedeemOffersAllowed(
          web_contents->GetTopLevelNativeWindow(),
          params->consent_requester.service_name,
          params->consent_requester.origin,
          base::BindOnce(&EchoPrivateGetUserConsentFunction::Finalize, this));
  return RespondLater();
}

void EchoPrivateGetUserConsentFunction::Finalize(bool consent) {
  Respond(WithArguments(consent));
}
