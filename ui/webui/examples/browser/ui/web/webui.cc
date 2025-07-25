// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/web/webui.h"

#include "chrome/grit/webui_gallery_resources.h"
#include "chrome/grit/webui_gallery_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ui_base_features.h"
#include "ui/webui/examples/grit/webui_examples_resources.h"

namespace webui_examples {

namespace {

void EnableTrustedTypesCSP(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types parse-html-subset sanitize-inner-html static-types "
      // Add TrustedTypes policies for cr-lottie.
      "lottie-worker-script-loader "
      // Add TrustedTypes policies necessary for using Polymer.
      "polymer-html-literal polymer-template-event-attribute-policy;");
}

void SetJSModuleDefaults(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      "frame-ancestors 'self';");
}

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  SetJSModuleDefaults(source);
  EnableTrustedTypesCSP(source);
  source->AddString(
      "chromeRefresh2023Attribute",
      features::IsChromeWebuiRefresh2023() ? "chrome-refresh-2023" : "");
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
}

}  // namespace

WebUI::WebUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kHost);
  SetupWebUIDataSource(
      source,
      base::make_span(kWebuiGalleryResources, kWebuiGalleryResourcesSize),
      IDR_WEBUI_GALLERY_WEBUI_GALLERY_HTML);
}

WebUI::~WebUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebUI)

}  // namespace webui_examples
