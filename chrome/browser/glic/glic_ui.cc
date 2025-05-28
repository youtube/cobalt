// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_ui.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/glic/fre_util.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_fre_page_handler.h"
#include "chrome/browser/glic/glic_page_handler.h"
#include "chrome/browser/glic/guest_util.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_resources.h"
#include "chrome/grit/glic_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_allowlist.h"
#include "ui/webui/webui_util.h"

namespace glic {

// static
bool GlicUI::simulate_no_connection_ = false;

GlicUIConfig::GlicUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIGlicHost) {}

bool GlicUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return GlicEnabling::IsProfileEligible(
      Profile::FromBrowserContext(browser_context));
}

GlicUI::GlicUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"errorNotice", IDS_GLIC_ERROR_NOTICE},
      {"errorNoticeActionButton", IDS_GLIC_ERROR_NOTICE_ACTION_BUTTON},
      {"errorNoticeHeader", IDS_GLIC_ERROR_NOTICE_HEADER},
      {"offlineNoticeAction", IDS_GLIC_OFFLINE_NOTICE_ACTION},
      {"offlineNoticeActionButton", IDS_GLIC_OFFLINE_NOTICE_ACTION_BUTTON},
      {"offlineNoticeHeader", IDS_GLIC_OFFLINE_NOTICE_HEADER},
  };

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Set up the chrome://glic source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIGlicHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kGlicResources, IDR_GLIC_GLIC_HTML);

  // Add localized strings.
  source->AddLocalizedStrings(kStrings);

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  allowlist->RegisterAutoGrantedPermission(source->GetOrigin(),
                                           ContentSettingsType::GEOLOCATION);

  auto* command_line = base::CommandLine::ForCurrentProcess();

  // Set up guest URL via cli flag or default to finch param value.
  source->AddString("glicGuestURL", GetGuestURL().spec());

  // Set up FRE URL via cli flag, or default to the finch param value.
  source->AddString("glicFreURL", GetFreURL().spec());

  // Set up loading notice timeout values.
  source->AddInteger("preLoadingTimeMs", features::kGlicPreLoadingTimeMs.Get());
  source->AddInteger("minLoadingTimeMs", features::kGlicMinLoadingTimeMs.Get());
  source->AddInteger("maxLoadingTimeMs", features::kGlicMaxLoadingTimeMs.Get());
  source->AddBoolean("simulateNoConnection", simulate_no_connection_);

  source->AddResourcePath("glic_logo.svg", IDR_GLIC_LOGO);

  // Set up guest api source.
  // This comes from 'glic_api_injection' in
  // chrome/browser/resources/glic/BUILD.gn.
  source->AddString(
      "glicGuestAPISource",
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GLIC_GLIC_API_IMPL_GLIC_API_INJECTED_CLIENT_ROLLUP_JS));

  std::string allowed_origins =
      command_line->GetSwitchValueASCII(::switches::kGlicAllowedOrigins);
  if (allowed_origins.empty()) {
    allowed_origins = features::kGlicAllowedOriginsOverride.Get();
  }
  if (allowed_origins.empty()) {
    // TODO(crbug.com/396147389): Replace with the correct default.
    allowed_origins = "https://*.google.com/";
  }
  source->AddString("glicAllowedOrigins", allowed_origins);

  source->AddBoolean("enableDebug",
                     base::FeatureList::IsEnabled(features::kGlicDebugWebview));

  source->AddBoolean("enableScrollTo",
                     base::FeatureList::IsEnabled(features::kGlicScrollTo));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicUI)

GlicUI::~GlicUI() = default;

void GlicUI::BindInterface(
    mojo::PendingReceiver<glic::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicUI::CreatePageHandler(
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<glic::mojom::Page> page) {
  page_handler_ = std::make_unique<GlicPageHandler>(
      web_ui()->GetWebContents(), std::move(receiver), std::move(page));
}

void GlicUI::CreateFrePageHandler(
    mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver) {
  fre_page_handler_ = std::make_unique<GlicFrePageHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

}  // namespace glic
