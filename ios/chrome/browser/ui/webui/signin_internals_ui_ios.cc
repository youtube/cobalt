// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/signin_internals_ui_ios.h"

#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/about_signin_internals_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/url/chrome_url_constants.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"

namespace {

web::WebUIIOSDataSource* CreateSignInInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUISignInInternalsHost);

  source->UseStringsJs();
  source->AddResourcePath("signin_internals.js", IDR_SIGNIN_INTERNALS_INDEX_JS);
  source->AddResourcePath("signin_index.css", IDR_SIGNIN_INTERNALS_INDEX_CSS);
  source->SetDefaultResource(IDR_SIGNIN_INTERNALS_INDEX_HTML);

  return source;
}

}  //  namespace

SignInInternalsUIIOS::SignInInternalsUIIOS(web::WebUIIOS* web_ui,
                                           const std::string& host)
    : WebUIIOSController(web_ui, host) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  DCHECK(browser_state);
  web::WebUIIOSDataSource::Add(browser_state,
                               CreateSignInInternalsHTMLSource());
  web_ui->AddMessageHandler(std::make_unique<SignInInternalsHandlerIOS>());
}

SignInInternalsUIIOS::~SignInInternalsUIIOS() = default;

SignInInternalsHandlerIOS::SignInInternalsHandlerIOS() {}

SignInInternalsHandlerIOS::~SignInInternalsHandlerIOS() {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  DCHECK(browser_state);
  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);
  if (about_signin_internals)
    about_signin_internals->RemoveSigninObserver(this);
}

void SignInInternalsHandlerIOS::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSigninInfo",
      base::BindRepeating(&SignInInternalsHandlerIOS::HandleGetSignInInfo,
                          base::Unretained(this)));
}

void SignInInternalsHandlerIOS::HandleGetSignInInfo(
    const base::Value::List& args) {
  CHECK_GE(args.size(), 1u);
  std::string callback_id = args[0].GetString();  // CHECKs if non-string.
  base::Value callback(callback_id);
  base::Value success(true);

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  DCHECK(browser_state);
  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);

  if (!about_signin_internals) {
    base::Value empty;
    base::ValueView return_args[] = {callback, success, empty};
    web_ui()->CallJavascriptFunction("cr.webUIResponse", return_args);
    return;
  }

  // Note(vishwath): The UI would look better if we passed in a dict with some
  // reasonable defaults, so the about:signin-internals page doesn't look
  // empty in incognito mode. Alternatively, we could force about:signin to
  // open in non-incognito mode always (like about:settings for ex.).
  about_signin_internals->AddSigninObserver(this);
  const base::Value::Dict status = about_signin_internals->GetSigninStatus();
  base::ValueView return_args[] = {callback, success, status};
  web_ui()->CallJavascriptFunction("cr.webUIResponse", return_args);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar.accounts_are_fresh) {
    about_signin_internals->OnAccountsInCookieUpdated(
        accounts_in_cookie_jar,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void SignInInternalsHandlerIOS::OnSigninStateChanged(
    const base::Value::Dict& info) {
  base::Value event_name("signin-info-changed");
  base::ValueView args[] = {event_name, info};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

void SignInInternalsHandlerIOS::OnCookieAccountsFetched(
    const base::Value::Dict& info) {
  base::Value event_name("update-cookie-accounts");
  base::ValueView args[] = {event_name, info};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}
