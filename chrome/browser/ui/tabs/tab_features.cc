// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/fingerprinting_protection/chrome_fingerprinting_protection_web_contents_helper_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/passage_embeddings/embedder_tab_observer.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_tab_observer.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/disconnect_file_chooser_on_background_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_translate_action_listener.h"
#include "chrome/browser/ui/views/page_action/action_ids.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/translate/translate_page_action_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/metrics/content/dwa_web_contents_observer.h"
#include "components/permissions/permission_indicators_tab_data.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_tab_indicator_helper.h"
#endif
namespace tabs {

namespace {

// This is the generic entry point for test code to stub out TabFeature
// functionality. It is called by production code, but only used by tests.
TabFeatures::TabFeaturesFactory& GetFactory() {
  static base::NoDestructor<TabFeatures::TabFeaturesFactory> factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<TabFeatures> TabFeatures::CreateTabFeatures() {
  if (GetFactory()) {
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new TabFeatures());
}

TabFeatures::~TabFeatures() = default;

// static
void TabFeatures::ReplaceTabFeaturesForTesting(TabFeaturesFactory factory) {
  TabFeatures::TabFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void TabFeatures::Init(TabInterface& tab, Profile* profile) {
  CHECK(!initialized_);
  initialized_ = true;

  // In tests you may want to disable TabFeatures initialization.
  // See tabs::PreventTabFeatureInitialization
  CHECK(tab.GetBrowserWindowInterface());

  tab_subscriptions_.push_back(
      tab.RegisterWillDiscardContents(base::BindRepeating(
          &TabFeatures::WillDiscardContents, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(webui::InitEmbeddingContext(&tab));

  // TODO(crbug.com/346148554): Do not create a SidePanelRegistry or
  // dependencies for non-normal browsers.
  side_panel_registry_ = std::make_unique<SidePanelRegistry>(&tab);

  // Features that are only enabled for normal browser windows. By default most
  // features should be instantiated in this block.
  if (tab.IsInNormalWindow()) {
    lens_overlay_controller_ = CreateLensController(&tab, profile);

    // Each time a new tab is created, validate the topics calculation schedule
    // to help investigate a scheduling bug (crbug.com/343750866).
    if (browsing_topics::BrowsingTopicsService* browsing_topics_service =
            browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
                profile)) {
      browsing_topics_service->ValidateCalculationSchedule();
    }

    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab.GetContents());

    chrome_autofill_ai_client_ =
        ChromeAutofillAiClient::MaybeCreateForWebContents(tab.GetContents(),
                                                          profile);

    pinned_translate_action_listener_ =
        std::make_unique<PinnedTranslateActionListener>(&tab);

    if (!profile->IsIncognitoProfile()) {
      commerce_ui_tab_helper_ =
          CreateCommerceUiTabHelper(tab.GetContents(), profile);
    }

    contextual_cueing::ContextualCueingHelper::MaybeCreateForWebContents(
        tab.GetContents());

    privacy_sandbox_tab_observer_ =
        std::make_unique<privacy_sandbox::PrivacySandboxTabObserver>(
            tab.GetContents());

    dwa_web_contents_observer_ =
        std::make_unique<metrics::DwaWebContentsObserver>(
            tab.GetContents());

    if (tab_groups::TabGroupSyncService* tab_group_sync_service =
            tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile)) {
      saved_tab_group_web_contents_listener_ =
          std::make_unique<tab_groups::SavedTabGroupWebContentsListener>(
              tab_group_sync_service, &tab);
    }

    if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
      collaboration_messaging_tab_data_ =
          std::make_unique<tab_groups::CollaborationMessagingTabData>(profile);
    }

    embedder_tab_observer_ =
        std::make_unique<passage_embeddings::EmbedderTabObserver>(
            tab.GetContents());

#if BUILDFLAG(ENABLE_GLIC)
    if (glic::GlicEnabling::IsProfileEligible(
            tab.GetBrowserWindowInterface()->GetProfile())) {
      glic_tab_indicator_helper_ =
          std::make_unique<glic::GlicTabIndicatorHelper>(&tab);
    }
#endif  // BUILDFLAG(ENABLE_GLIC)
  }     // IsInNormalWindow() end.

  auto* pinned_actions_model = PinnedToolbarActionsModel::Get(profile);
  CHECK(pinned_actions_model);
  page_action_controller_ =
      std::make_unique<page_actions::PageActionController>(
          pinned_actions_model);
  page_action_controller_->Initialize(
      tab, std::vector<actions::ActionId>(page_actions::kActionIds.begin(),
                                          page_actions::kActionIds.end()));
  translate_page_action_controller_ =
      std::make_unique<TranslatePageActionController>(tab);

  customize_chrome_side_panel_controller_ =
      std::make_unique<customize_chrome::SidePanelControllerViews>(tab);

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          profile, &tab, side_panel_registry_.get());

  tab_dialog_manager_ = std::make_unique<TabDialogManager>(&tab);

  data_protection_controller_ = std::make_unique<
      enterprise_data_protection::DataProtectionNavigationController>(&tab);

  // TODO(https://crbug.com/355485153): Move this into the normal window block.
  read_anything_side_panel_controller_ =
      std::make_unique<ReadAnythingSidePanelController>(
          &tab, side_panel_registry_.get());

  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionEnabledForIncognitoState(
              profile->IsIncognitoProfile())) {
    CreateFingerprintingProtectionWebContentsHelper(
        tab.GetContents(), profile->GetPrefs(),
        HostContentSettingsMapFactory::GetForProfile(profile),
        TrackingProtectionSettingsFactory::GetForProfile(profile),
        profile->IsIncognitoProfile());
  }

  if (web_app::AreWebAppsEnabled(profile)) {
    web_app::WebAppTabHelper::Create(&tab, tab.GetContents());
  }

  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          tab.GetContents(),
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(tab.GetContents()),
          favicon::ContentFaviconDriver::FromWebContents(tab.GetContents()));

  task_manager::WebContentsTags::CreateForTabContents(tab.GetContents());

  if (base::FeatureList::IsEnabled(
          tabs::kDisconnectFileChooserOnTabDeactivateKillSwitch)) {
    disconnect_file_chooser_on_background_controller_ =
        std::make_unique<DisconnectFileChooserOnBackgroundController>(tab);
  }
}

TabFeatures::TabFeatures() = default;

std::unique_ptr<LensOverlayController> TabFeatures::CreateLensController(
    TabInterface* tab,
    Profile* profile) {
  return std::make_unique<LensOverlayController>(
      tab, profile->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      SyncServiceFactory::GetForProfile(profile),
      ThemeServiceFactory::GetForProfile(profile));
}

std::unique_ptr<commerce::CommerceUiTabHelper>
TabFeatures::CreateCommerceUiTabHelper(content::WebContents* web_contents,
                                       Profile* profile) {
  // TODO(crbug.com/40863325): Consider using the in-memory cache instead.
  return std::make_unique<commerce::CommerceUiTabHelper>(
      web_contents,
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
      BookmarkModelFactory::GetForBrowserContext(profile),
      ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly),
      side_panel_registry_.get());
}

void TabFeatures::WillDiscardContents(tabs::TabInterface* tab,
                                      content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, tab->GetContents());

  Profile* profile = tab->GetBrowserWindowInterface()->GetProfile();

  // This method is transiently used to reset features that do not handle tab
  // discarding themselves.
  read_anything_side_panel_controller_->ResetForTabDiscard();
  read_anything_side_panel_controller_.reset();
  read_anything_side_panel_controller_ =
      std::make_unique<ReadAnythingSidePanelController>(
          tab, side_panel_registry_.get());

  // Deregister side-panel entries that are web-contents scoped rather than tab
  // scoped.
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));

  if (commerce_ui_tab_helper_) {
    commerce_ui_tab_helper_.reset();
    commerce_ui_tab_helper_ = CreateCommerceUiTabHelper(new_contents, profile);
  }
  if (chrome_autofill_ai_client_) {
    chrome_autofill_ai_client_ =
        ChromeAutofillAiClient::MaybeCreateForWebContents(new_contents,
                                                          profile);
  }

  if (privacy_sandbox_tab_observer_) {
    privacy_sandbox_tab_observer_.reset();
    privacy_sandbox_tab_observer_ =
        std::make_unique<privacy_sandbox::PrivacySandboxTabObserver>(
            new_contents);
  }

  if (dwa_web_contents_observer_) {
    dwa_web_contents_observer_.reset();
    dwa_web_contents_observer_ =
        std::make_unique<metrics::DwaWebContentsObserver>(new_contents);
  }

  if (web_app::AreWebAppsEnabled(
          tab->GetBrowserWindowInterface()->GetProfile())) {
    web_app::WebAppTabHelper::Create(tab, new_contents);
  }

  sync_sessions_router_.reset();
  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          new_contents,
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(new_contents),
          favicon::ContentFaviconDriver::FromWebContents(new_contents));

  if (permission_indicators_tab_data_) {
    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            new_contents);
  }
}

}  // namespace tabs
