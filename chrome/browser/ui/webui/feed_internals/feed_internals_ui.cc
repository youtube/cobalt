// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/feed_internals_resources.h"
#include "chrome/grit/feed_internals_resources_map.h"
#include "components/feed/buildflags.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if BUILDFLAG(ENABLE_FEED_V2)
#include "chrome/browser/ui/webui/feed_internals/feedv2_internals_page_handler.h"
#endif

FeedInternalsUI::FeedInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUISnippetsInternalsHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kFeedInternalsResources, kFeedInternalsResourcesSize),
      IDR_FEED_INTERNALS_FEED_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(FeedInternalsUI)

FeedInternalsUI::~FeedInternalsUI() = default;

void FeedInternalsUI::BindInterface(
    mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver) {
#if BUILDFLAG(ENABLE_FEED_V2)
  v2_page_handler_ = std::make_unique<FeedV2InternalsPageHandler>(
      std::move(receiver),
      feed::FeedServiceFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs());
#endif
}
