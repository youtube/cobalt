// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/office_fallback_resources.h"
#include "chrome/grit/office_fallback_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::office_fallback {

bool OfficeFallbackUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud();
}

OfficeFallbackUI::OfficeFallbackUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOfficeFallbackHost);

  // Set text for dialog buttons.
  static constexpr webui::LocalizedString kStrings[] = {
      {"officeFallbackCancel", IDS_OFFICE_FALLBACK_CANCEL},
      {"officeFallbackTryAgain", IDS_OFFICE_FALLBACK_TRY_AGAIN},
      {"officeFallbackOpenWithOfflineEditor",
       IDS_OFFICE_FALLBACK_OPEN_WITH_OFFLINE_EDITOR},
  };
  source->AddLocalizedStrings(kStrings);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kOfficeFallbackResources, kOfficeFallbackResourcesSize),
      IDR_OFFICE_FALLBACK_MAIN_HTML);
  source->DisableTrustedTypesCSP();
}

OfficeFallbackUI::~OfficeFallbackUI() = default;

void OfficeFallbackUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (factory_receiver_.is_bound()) {
    factory_receiver_.reset();
  }
  factory_receiver_.Bind(std::move(pending_receiver));
}

void OfficeFallbackUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<OfficeFallbackPageHandler>(
      std::move(receiver),
      // base::Unretained() because |page_handler_| will not out-live |this|.
      base::BindOnce(&OfficeFallbackUI::CloseDialog, base::Unretained(this)));
}

void OfficeFallbackUI::CloseDialog(mojom::DialogChoice choice) {
  base::Value::List args;
  switch (choice) {
    case mojom::DialogChoice::kCancel:
      args.Append(kDialogChoiceCancel);
      break;
    case mojom::DialogChoice::kQuickOffice:
      args.Append(kDialogChoiceQuickOffice);
      break;
    case mojom::DialogChoice::kTryAgain:
      args.Append(kDialogChoiceTryAgain);
      break;
  }
  ui::MojoWebDialogUI::CloseDialog(args);
}

WEB_UI_CONTROLLER_TYPE_IMPL(OfficeFallbackUI)

}  // namespace ash::office_fallback
