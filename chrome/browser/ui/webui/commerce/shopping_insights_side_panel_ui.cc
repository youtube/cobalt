// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"

#include <memory>

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/commerce/price_insights_handler.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_commerce_resources.h"
#include "chrome/grit/side_panel_commerce_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mojom/shopping_service.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_util.h"

ShoppingInsightsSidePanelUI::ShoppingInsightsSidePanelUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, commerce::kChromeUIShoppingInsightsSidePanelHost);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"historyTitle", IDS_PRICE_HISTORY_TITLE},
      {"historyDescription", IDS_PRICE_HISTORY_DESCRIPTION},
      {"lowPriceMultipleOptions", IDS_PRICE_HISTORY_MULTIPLE_OPTIONS_LOW_PRICE},
      {"highPriceMultipleOptions",
       IDS_PRICE_HISTORY_MULTIPLE_OPTIONS_HIGH_PRICE},
      {"typicalPriceMultipleOptions",
       IDS_PRICE_HISTORY_MULTIPLE_OPTIONS_TYPICAL_PRICE},
      {"lowPriceSingleOption", IDS_PRICE_HISTORY_SINGLE_OPTION_LOW_PRICE},
      {"highPriceSingleOption", IDS_PRICE_HISTORY_SINGLE_OPTION_HIGH_PRICE},
      {"typicalPriceSingleOption",
       IDS_PRICE_HISTORY_SINGLE_OPTION_TYPICAL_PRICE},
      {"buyOptions", IDS_SHOPPING_INSIGHTS_BUYING_OPTIONS},
      {"feedback", IDS_SHOPPING_INSIGHTS_COLLECT_FEEDBACK},
      {"rangeMultipleOptions", IDS_PRICE_RANGE_ALL_OPTIONS},
      {"rangeMultipleOptionsOnePrice",
       IDS_PRICE_RANGE_ALL_OPTIONS_ONE_TYPICAL_PRICE},
      {"rangeSingleOption", IDS_PRICE_RANGE_SINGLE_OPTION},
      {"rangeSingleOptionOnePrice",
       IDS_PRICE_RANGE_SINGLE_OPTION_ONE_TYPICAL_PRICE},
      {"trackPriceTitle", IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TRACK_PRICE_TITLE},
      {"trackPriceDescription",
       IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TRACK_PRICE_DESCRIPTION},
      {"trackPriceSaveDescription", IDS_PRICE_TRACKING_SAVE_DESCRIPTION},
      {"trackPriceSaveLocation", IDS_PRICE_TRACKING_SAVE_LOCATION},
      {"trackPriceError", IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TRACK_PRICE_ERROR},
      {"yesterday", IDS_PRICE_HISTORY_YESTERDAY_PRICE},
      {"historyGraphAccessibility", IDS_PRICE_HISTORY_GRAPH_ACCESSIBILITY},
      {"historyTitleMultipleOptions", IDS_PRICE_HISTORY_TITLE_MULTIPLE_OPTIONS},
      {"historyTitleSingleOption", IDS_PRICE_HISTORY_TITLE_SINGLE_OPTION},
  };
  for (const auto& str : kLocalizedStrings) {
    webui::AddLocalizedString(source, str.name, str.id);
  }

  source->AddBoolean("shouldShowFeedback",
                     commerce::kPriceInsightsShowFeedback.Get());

  webui::SetupWebUIDataSource(source, kSidePanelCommerceResources,
                              IDR_SIDE_PANEL_COMMERCE_SHOPPING_INSIGHTS_HTML);
  source->AddResourcePaths(kSidePanelSharedResources);
}

ShoppingInsightsSidePanelUI::~ShoppingInsightsSidePanelUI() = default;

void ShoppingInsightsSidePanelUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ShoppingInsightsSidePanelUI::BindInterface(
    mojo::PendingReceiver<
        shopping_service::mojom::ShoppingServiceHandlerFactory> receiver) {
  shopping_service_factory_receiver_.reset();
  shopping_service_factory_receiver_.Bind(std::move(receiver));
}

void ShoppingInsightsSidePanelUI::BindInterface(
    mojo::PendingReceiver<
        commerce::price_tracking::mojom::PriceTrackingHandlerFactory>
        receiver) {
  price_tracking_factory_receiver_.reset();
  price_tracking_factory_receiver_.Bind(std::move(receiver));
}

void ShoppingInsightsSidePanelUI::BindInterface(
    mojo::PendingReceiver<
        commerce::price_insights::mojom::PriceInsightsHandlerFactory>
        receiver) {
  price_insights_factory_receiver_.reset();
  price_insights_factory_receiver_.Bind(std::move(receiver));
}

void ShoppingInsightsSidePanelUI::CreateShoppingServiceHandler(
    mojo::PendingReceiver<shopping_service::mojom::ShoppingServiceHandler>
        receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  shopping_service_handler_ =
      std::make_unique<commerce::ShoppingServiceHandler>(
          std::move(receiver), bookmark_model, shopping_service,
          profile->GetPrefs(), tracker,
          std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile),
          nullptr);
}

void ShoppingInsightsSidePanelUI::CreatePriceTrackingHandler(
    mojo::PendingRemote<commerce::price_tracking::mojom::Page> page,
    mojo::PendingReceiver<commerce::price_tracking::mojom::PriceTrackingHandler>
        receiver) {
  Profile* const profile = Profile::FromWebUI(web_ui());
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  price_tracking_handler_ = std::make_unique<commerce::PriceTrackingHandler>(
      std::move(page), std::move(receiver), web_ui(), shopping_service, tracker,
      bookmark_model);
}

void ShoppingInsightsSidePanelUI::CreatePriceInsightsHandler(
    mojo::PendingReceiver<commerce::price_insights::mojom::PriceInsightsHandler>
        receiver) {
  price_insights_handler_ = std::make_unique<commerce::PriceInsightsHandler>(
      std::move(receiver), *this, Profile::FromWebUI(web_ui()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ShoppingInsightsSidePanelUI)
