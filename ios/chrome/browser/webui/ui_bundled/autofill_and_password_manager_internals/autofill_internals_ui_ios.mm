// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/autofill_and_password_manager_internals/autofill_internals_ui_ios.h"

#import "ios/chrome/browser/autofill/model/autofill_log_router_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/autofill_and_password_manager_internals/internals_ui_handler.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

using autofill::LogRouter;

AutofillInternalsUIIOS::AutofillInternalsUIIOS(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile, autofill::CreateInternalsHTMLSource(
                                            kChromeUIAutofillInternalsHost));
  web_ui->AddMessageHandler(std::make_unique<autofill::InternalsUIHandler>(
      "setup-autofill-internals",
      base::BindRepeating(&autofill::AutofillLogRouterFactory::GetForProfile)));
}

AutofillInternalsUIIOS::~AutofillInternalsUIIOS() = default;
