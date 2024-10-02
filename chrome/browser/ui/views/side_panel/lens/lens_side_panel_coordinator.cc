// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include <iostream>

#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/vector_icons.h"

LensSidePanelCoordinator::LensSidePanelCoordinator(Browser* browser)
    : BrowserUserData(*browser) {
  GetBrowserView()->side_panel_coordinator()->AddSidePanelViewStateObserver(
      this);
  lens_side_panel_view_ = nullptr;
  auto* profile = GetBrowserView()->GetProfile();
  favicon_cache_ = std::make_unique<FaviconCache>(
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile);
  current_default_search_provider_ =
      template_url_service_->GetDefaultSearchProvider();
  template_url_service_->AddObserver(this);
}

BrowserView* LensSidePanelCoordinator::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

LensSidePanelCoordinator::~LensSidePanelCoordinator() {
  if (GetBrowserView() && GetBrowserView()->side_panel_coordinator()) {
    GetBrowserView()
        ->side_panel_coordinator()
        ->RemoveSidePanelViewStateObserver(this);
  }

  if (template_url_service_ != nullptr)
    template_url_service_->RemoveObserver(this);
}

void LensSidePanelCoordinator::DeregisterLensFromSidePanel() {
  lens_side_panel_view_ = nullptr;
  SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser())
      ->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kLens));
}

void LensSidePanelCoordinator::OnSidePanelDidClose() {
  DeregisterLensFromSidePanel();
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.HideSidePanel"));
}

void LensSidePanelCoordinator::OnFaviconFetched(const gfx::Image& favicon) {
  auto* global_registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser());
  if (global_registry == nullptr)
    return;

  auto* lens_side_panel_entry = global_registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kLens));
  if (lens_side_panel_entry == nullptr)
    return;

  lens_side_panel_entry->ResetIcon(ui::ImageModel::FromImage(favicon));
}

void LensSidePanelCoordinator::OnTemplateURLServiceChanged() {
  // When the default search engine changes, remove lens from the side panel to
  // avoid a potentially mismatched state between the combo box label and lens
  // side panel content.
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  if (default_search_provider == current_default_search_provider_)
    return;

  current_default_search_provider_ = default_search_provider;
  DeregisterLensFromSidePanel();

  base::RecordAction(base::UserMetricsAction(
      "LensUnifiedSidePanel.RemoveLensEntry_SearchEngineChanged"));
}

void LensSidePanelCoordinator::OnEntryShown(SidePanelEntry* entry) {
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.LensEntryShown"));
}

void LensSidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.LensEntryHidden"));
}

bool LensSidePanelCoordinator::IsLaunchButtonEnabledForTesting() {
  DCHECK(lens_side_panel_view_);
  return lens_side_panel_view_->IsLaunchButtonEnabledForTesting();
}

bool LensSidePanelCoordinator::IsDefaultSearchProviderGoogle() {
  auto* profile = GetBrowserView()->GetProfile();
  return search::DefaultSearchProviderIsGoogle(profile);
}

std::u16string LensSidePanelCoordinator::GetComboboxLabel() {
  if (IsDefaultSearchProviderGoogle()) {
    return l10n_util::GetStringUTF16(IDS_GOOGLE_LENS_TITLE);
  }
  // Assuming not nullptr because side panel can't be opened if default search
  // provider is not initialized
  DCHECK(current_default_search_provider_);
  return current_default_search_provider_->image_search_branding_label();
}

const ui::ImageModel LensSidePanelCoordinator::GetFaviconImage() {
  // If google is search engine, return checked-in lens icon.
  if (IsDefaultSearchProviderGoogle())
    return ui::ImageModel::FromVectorIcon(vector_icons::kGoogleLensLogoIcon);

  auto default_image = ui::ImageModel::FromVectorIcon(
      vector_icons::kImageSearchIcon, ui::kColorIcon);

  // Return default icon if the search provider is nullptr.
  if (template_url_service_ == nullptr ||
      template_url_service_->GetDefaultSearchProvider() == nullptr) {
    return default_image;
  }

  // Use favicon URL for image search favicon for 3P DSE to avoid latency in
  // showing the icon.
  auto url = template_url_service_->GetDefaultSearchProvider()->favicon_url();
  // Return default icon if the favicon url is empty.
  if (url.is_empty())
    return default_image;

  auto image = favicon_cache_->GetFaviconForIconUrl(
      url, base::BindRepeating(&LensSidePanelCoordinator::OnFaviconFetched,
                               weak_ptr_factory_.GetWeakPtr()));

  // Return default icon if the icon returned from cache is empty.
  return image.IsEmpty() ? std::move(default_image)
                         : ui::ImageModel::FromImage(image);
}

void LensSidePanelCoordinator::RegisterEntryAndShow(
    const content::OpenURLParams& params) {
  base::RecordAction(base::UserMetricsAction("LensUnifiedSidePanel.LensQuery"));
  auto* global_registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser());

  // check if the view is already registered
  if (global_registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLens)) != nullptr &&
      lens_side_panel_view_ != nullptr) {
    // The user issued a follow-up Lens query.
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensQuery_Followup"));
    lens_side_panel_view_->OpenUrl(params);
  } else {
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensQuery_New"));
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens, GetComboboxLabel(), GetFaviconImage(),
        base::BindRepeating(&LensSidePanelCoordinator::CreateLensWebView,
                            base::Unretained(this), params),
        base::BindRepeating(&LensSidePanelCoordinator::GetOpenInNewTabURL,
                            base::Unretained(this)));
    entry->AddObserver(this);
    global_registry->Register(std::move(entry));
  }

  auto* side_panel_coordinator = GetBrowserView()->side_panel_coordinator();
  if (side_panel_coordinator->GetCurrentEntryId() !=
      SidePanelEntry::Id::kLens) {
    if (!side_panel_coordinator->IsSidePanelShowing()) {
      base::RecordAction(base::UserMetricsAction(
          "LensUnifiedSidePanel.LensQuery_SidePanelClosed"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "LensUnifiedSidePanel.LensQuery_SidePanelOpenNonLens"));
    }

    side_panel_coordinator->Show(SidePanelEntry::Id::kLens);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "LensUnifiedSidePanel.LensQuery_SidePanelOpenLens"));
  }
}

content::WebContents* LensSidePanelCoordinator::GetViewWebContentsForTesting() {
  return lens_side_panel_view_ ? lens_side_panel_view_->GetWebContents()
                               : nullptr;
}

bool LensSidePanelCoordinator::OpenResultsInNewTabForTesting() {
  if (lens_side_panel_view_ == nullptr)
    return false;

  lens_side_panel_view_->LoadResultsInNewTab();
  return true;
}

std::unique_ptr<views::View> LensSidePanelCoordinator::CreateLensWebView(
    const content::OpenURLParams& params) {
  auto side_panel_view_ = std::make_unique<lens::LensUnifiedSidePanelView>(
      GetBrowserView(),
      base::BindRepeating(&LensSidePanelCoordinator::UpdateNewTabButtonState,
                          base::Unretained(this)));
  side_panel_view_->OpenUrl(params);
  lens_side_panel_view_ = side_panel_view_->GetWeakPtr();
  return side_panel_view_;
}

GURL LensSidePanelCoordinator::GetOpenInNewTabURL() const {
  // If our view is null, then pass an invalid URL to side panel coordinator.
  if (lens_side_panel_view_ == nullptr)
    return GURL();

  return lens_side_panel_view_->GetOpenInNewTabURL();
}

void LensSidePanelCoordinator::UpdateNewTabButtonState() {
  if (GetBrowserView())
    GetBrowserView()->side_panel_coordinator()->UpdateNewTabButtonState();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensSidePanelCoordinator);
