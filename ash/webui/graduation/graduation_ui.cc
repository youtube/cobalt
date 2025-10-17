// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/graduation/graduation_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/edusumer/graduation_utils.h"
#include "ash/public/cpp/graduation/graduation_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/graduation/graduation_ui_handler.h"
#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "ash/webui/graduation/webview_auth_handler.h"
#include "ash/webui/grit/ash_graduation_resources.h"
#include "ash/webui/grit/ash_graduation_resources_map.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "base/version_info/version_info.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/resources/grit/webui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::graduation {

namespace {

// A string that indicates the webview is coming from CrOS. This user
// agent allows the Takeout Transfer embedded site to be loaded by the
// Graduation app. Ex. `ChromeWebView/133`.
constexpr char kUserAgentStringPrefix[] = "ChromeWebView/%s";

const std::string GetTransferUrl() {
  const std::string language_code = GraduationManager::Get()->GetLanguageCode();

  std::string url_base;
  // TODO(crbug.com/376690096): Clean up feature flag when the new
  // endpoint is launched.
  if (features::IsGraduationUseEmbeddedTransferEndpointEnabled()) {
    url_base = kEmbeddedTransferURLBase;
  } else {
    url_base = kTransferURLBase;
  }
  const GURL transfer_url_base(url_base);

  GURL::Replacements replacements;
  const std::string query_string =
      base::StringPrintf("hl=%s", language_code.c_str());
  replacements.SetQueryStr(query_string);
  const GURL transfer_url = transfer_url_base.ReplaceComponents(replacements);

  CHECK(transfer_url.is_valid())
      << "Invalid URL for Takeout Transfer tool: \"" << transfer_url << "\"";
  return transfer_url.spec();
}

std::string GetUserAgentString() {
  return base::StringPrintf(kUserAgentStringPrefix,
                            version_info::GetMajorVersionNumber());
}

void AddResources(content::WebUIDataSource* source) {
  source->SetDefaultResource(IDR_ASH_GRADUATION_INDEX_HTML);
  source->AddResourcePaths(kAshGraduationResources);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appTitle", IDS_GRADUATION_APP_TITLE},
      {"backButtonLabel", IDS_GRADUATION_APP_BACK_BUTTON_LABEL},
      {"doneButtonLabel", IDS_GRADUATION_APP_DONE_BUTTON_LABEL},
      {"getStartedButtonLabel", IDS_GRADUATION_APP_GET_STARTED_BUTTON_LABEL},
      {"errorPageDescription", IDS_GRADUATION_APP_ERROR_PAGE_DESCRIPTION},
      {"errorPageTitle", IDS_GRADUATION_APP_ERROR_PAGE_TITLE},
      {"offlinePageDescription", IDS_GRADUATION_APP_OFFLINE_PAGE_DESCRIPTION},
      {"offlinePageTitle", IDS_GRADUATION_APP_OFFLINE_PAGE_TITLE},
      {"webviewLoadingMessage", IDS_GRADUATION_APP_WEBVIEW_LOADING_MESSAGE},
      {"welcomePageTitle", IDS_GRADUATION_APP_WELCOME_PAGE_TITLE},
      {"welcomePageDescription", IDS_GRADUATION_APP_WELCOME_PAGE_DESCRIPTION},
      {"welcomePageSubDescription",
       IDS_GRADUATION_APP_WELCOME_PAGE_SUB_DESCRIPTION}};

  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddBoolean(
      "isEmbeddedEndpointEnabled",
      features::IsGraduationUseEmbeddedTransferEndpointEnabled());

  source->AddString("startTransferUrl", GetTransferUrl());
  source->AddString("userAgentString", GetUserAgentString());

  // Set up test resources used in browser tests.
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}
}  // namespace

bool GraduationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsGraduationEnabled() && Shell::HasInstance() &&
         IsEligibleForGraduation(Shell::Get()
                                     ->session_controller()
                                     ->GetLastActiveUserPrefService());
}

GraduationUI::GraduationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIGraduationAppURL));
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kChromeUIGraduationAppHost));

  // Enable test resources.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  ash::EnableTrustedTypesCSP(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  AddResources(source);
}

GraduationUI::~GraduationUI() = default;

void GraduationUI::BindInterface(
    mojo::PendingReceiver<graduation_ui::mojom::GraduationUiHandler> receiver) {
  content::BrowserContext* context =
      web_ui()->GetWebContents()->GetBrowserContext();
  CHECK(context);
  const std::string host_name =
      web_ui()->GetWebContents()->GetVisibleURL().host();
  ui_handler_ = std::make_unique<GraduationUiHandler>(
      std::move(receiver),
      std::make_unique<WebviewAuthHandler>(context, host_name),
      CHECK_DEREF(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(context)));
}

void GraduationUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GraduationUI)

}  // namespace ash::graduation
