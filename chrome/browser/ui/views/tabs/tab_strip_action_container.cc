// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/commerce/product_specifications_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_vector_icon_manager.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#endif  // BUILDFLAG(ENABLE_GLIC)
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TriggerOutcome {
  kAccepted = 0,
  kDismissed = 1,
  kTimedOut = 2,
  kMaxValue = kTimedOut,
};

constexpr base::TimeDelta kExpansionInDuration = base::Milliseconds(500);
constexpr base::TimeDelta kExpansionOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kOpacityInDuration = base::Milliseconds(300);
constexpr base::TimeDelta kOpacityOutDuration = base::Milliseconds(100);
constexpr base::TimeDelta kOpacityDelay = base::Milliseconds(100);
constexpr base::TimeDelta kShowDuration = base::Seconds(16);
constexpr char kAutoTabGroupsTriggerOutcomeName[] =
    "Tab.Organization.Trigger.Outcome";
constexpr char kDeclutterTriggerOutcomeName[] =
    "Tab.Organization.Declutter.Trigger.Outcome";
constexpr char kDeclutterTriggerBucketedCTRName[] =
    "Tab.Organization.Declutter.Trigger.BucketedCTR";
constexpr int kLargeSpaceBetweenButtons = 4;

}  // namespace

TabStripActionContainer::TabStripNudgeAnimationSession::
    TabStripNudgeAnimationSession(TabStripNudgeButton* button,
                                  TabStripActionContainer* container,
                                  AnimationSessionType session_type,
                                  base::OnceCallback<void()> on_animation_ended)
    : button_(button),
      container_(container),
      expansion_animation_(container),
      opacity_animation_(container),
      session_type_(session_type),
      on_animation_ended_(std::move(on_animation_ended)) {
  if (session_type_ == AnimationSessionType::HIDE) {
    expansion_animation_.Reset(1);
    opacity_animation_.Reset(1);
  }
}

TabStripActionContainer::TabStripNudgeAnimationSession::
    ~TabStripNudgeAnimationSession() = default;

void TabStripActionContainer::TabStripNudgeAnimationSession::Start() {
  if (session_type_ ==
      TabStripNudgeAnimationSession::AnimationSessionType::SHOW) {
    Show();
  } else {
    Hide();
  }
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ResetAnimationForTesting(double value) {
  if (opacity_animation_delay_timer_.IsRunning()) {
    opacity_animation_delay_timer_.FireNow();
  }

  expansion_animation_.Reset(value);
  opacity_animation_.Reset(value);
}

void TabStripActionContainer::TabStripNudgeAnimationSession::Show() {
  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionInDuration));
  opacity_animation_.SetSlideDuration(GetAnimationDuration(kOpacityInDuration));

  expansion_animation_.Show();

  const base::TimeDelta delay = GetAnimationDuration(kOpacityDelay);
  opacity_animation_delay_timer_.Start(
      FROM_HERE, delay, this,
      &TabStripActionContainer::TabStripNudgeAnimationSession::
          ShowOpacityAnimation);
}

void TabStripActionContainer::TabStripNudgeAnimationSession::Hide() {
  // Animate and hide existing chip.
  if (session_type_ ==
      TabStripNudgeAnimationSession::AnimationSessionType::SHOW) {
    if (opacity_animation_delay_timer_.IsRunning()) {
      opacity_animation_delay_timer_.FireNow();
    }
    session_type_ = TabStripNudgeAnimationSession::AnimationSessionType::HIDE;
  }

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionOutDuration));

  opacity_animation_.SetSlideDuration(
      GetAnimationDuration(kOpacityOutDuration));

  expansion_animation_.Hide();
  opacity_animation_.Hide();
}

base::TimeDelta
TabStripActionContainer::TabStripNudgeAnimationSession::GetAnimationDuration(
    base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ShowOpacityAnimation() {
  opacity_animation_.Show();
}

void TabStripActionContainer::TabStripNudgeAnimationSession::
    ApplyAnimationValue(const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    button_->SetWidthFactor(value);
  } else if (animation == &opacity_animation_) {
    button_->SetOpacity(value);
  }
}

void TabStripActionContainer::TabStripNudgeAnimationSession::MarkAnimationDone(
    const gfx::Animation* animation) {
  if (animation == &expansion_animation_) {
    expansion_animation_done_ = true;
  } else {
    opacity_animation_done_ = true;
  }

  if (expansion_animation_done_ && opacity_animation_done_) {
    if (on_animation_ended_) {
      std::move(on_animation_ended_).Run();
    }
  }
}

TabStripActionContainer::TabStripActionContainer(
    TabStripController* tab_strip_controller,
    View* locked_expansion_view,
    tabs::TabDeclutterController* tab_declutter_controller,
    tabs::GlicNudgeController* glic_nudge_controller)
    : AnimationDelegateViews(this),
      locked_expansion_view_(locked_expansion_view),
      tab_declutter_controller_(tab_declutter_controller),
      glic_nudge_controller_(glic_nudge_controller),
      tab_strip_controller_(tab_strip_controller) {
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view,
                                                    gfx::Insets()),
      this);

  tab_organization_service_ = TabOrganizationServiceFactory::GetForProfile(
      tab_strip_controller->GetProfile());
  if (tab_organization_service_) {
    tab_organization_observation_.Observe(tab_organization_service_);
  }

  // `tab_declutter_controller_` will be null for some profile types and if
  // feature is not enabled.
  if (tab_declutter_controller_) {
    tab_declutter_button_ =
        AddChildView(CreateTabDeclutterButton(tab_strip_controller));

    SetupButtonProperties(tab_declutter_button_);

    tab_declutter_observation_.Observe(tab_declutter_controller_);
  }

  // `glic_nudge_controller_` will be null if feature is not enabled.
  if (glic_nudge_controller_) {
#if BUILDFLAG(ENABLE_GLIC)
    glic_nudge_button_ =
        AddChildView(CreateGlicNudgeButton(tab_strip_controller));

    SetupButtonProperties(glic_nudge_button_);

    tab_glic_nudge_observation_.Observe(glic_nudge_controller_);
#else
    NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
  }
  auto_tab_group_button_ =
      AddChildView(CreateAutoTabGroupButton(tab_strip_controller));

  SetupButtonProperties(auto_tab_group_button_);

  browser_ = tab_strip_controller->GetBrowser();
  BrowserWindowInterface* browser_window_interface =
      tab_strip_controller->GetBrowserWindowInterface();
  if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    std::unique_ptr<ProductSpecificationsButton> product_specifications_button;
    product_specifications_button =
        std::make_unique<ProductSpecificationsButton>(
            tab_strip_controller, browser_window_interface->GetTabStripModel(),
            browser_window_interface->GetFeatures()
                .product_specifications_entry_point_controller(),
            /*render_tab_search_before_tab_strip_*/ false, this);
    product_specifications_button->SetProperty(views::kCrossAxisAlignmentKey,
                                               views::LayoutAlignment::kCenter);

    product_specifications_button_ =
        AddChildView(std::move(product_specifications_button));
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsProfileEligible(
          tab_strip_controller->GetProfile())) {
    glic_button_ = AddChildView(CreateGlicButton());
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabStripActionContainer::~TabStripActionContainer() {
  if (scoped_tab_strip_modal_ui_) {
    scoped_tab_strip_modal_ui_.reset();
  }
}

#if BUILDFLAG(ENABLE_GLIC)
std::unique_ptr<TabStripNudgeButton>
TabStripActionContainer::CreateGlicNudgeButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabStripActionContainer::OnGlicNudgeButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabStripActionContainer::OnGlicNudgeButtonDismissed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_GLIC_PROMO_TITLE),
      kGlicNudgeButtonElementId, Edge::kNone,
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON));

  button->SetTooltipText(l10n_util::GetStringUTF16(IDS_GLIC_PROMO_TITLE));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_GLIC_PROMO_TITLE));

  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}
#endif  // BUILDFLAG(ENABLE_GLIC)

std::unique_ptr<TabStripNudgeButton>
TabStripActionContainer::CreateTabDeclutterButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabStripActionContainer::OnTabDeclutterButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(
          &TabStripActionContainer::OnTabDeclutterButtonDismissed,
          base::Unretained(this)),
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER_NO_DEDUPE),
      kTabDeclutterButtonElementId, Edge::kNone, gfx::VectorIcon::EmptyIcon());

  button->SetTooltipText(
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER_NO_DEDUPE));
  button->GetViewAccessibility().SetName(
      features::IsTabstripDedupeEnabled()
          ? l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER)
          : l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER_NO_DEDUPE));

  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

std::unique_ptr<TabStripNudgeButton>
TabStripActionContainer::CreateAutoTabGroupButton(
    TabStripController* tab_strip_controller) {
  auto button = std::make_unique<TabStripNudgeButton>(
      tab_strip_controller,
      base::BindRepeating(&TabStripActionContainer::OnAutoTabGroupButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(
          &TabStripActionContainer::OnAutoTabGroupButtonDismissed,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE), kAutoTabGroupButtonElementId,
      Edge::kNone, gfx::VectorIcon::EmptyIcon());
  button->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE));
  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

#if BUILDFLAG(ENABLE_GLIC)
std::unique_ptr<glic::GlicButton> TabStripActionContainer::CreateGlicButton() {
  std::unique_ptr<glic::GlicButton> glic_button =
      std::make_unique<glic::GlicButton>(
          tab_strip_controller_,
          base::BindRepeating(&TabStripActionContainer::OnGlicButtonClicked,
                              base::Unretained(this)),
          glic::GlicVectorIconManager::GetVectorIcon(
              IDR_GLIC_BUTTON_VECTOR_ICON),
          l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP));

  glic_button->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kCenter);

  return glic_button;
}

void TabStripActionContainer::OnGlicButtonClicked() {
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  tab_strip_controller_->GetBrowserWindowInterface()
      ->GetUserEducationInterface()
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGlicPromoFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

  glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_strip_controller_->GetProfile())
      ->ToggleUI(tab_strip_controller_->GetBrowserWindowInterface(),
                 /*prevent_close=*/false,
                 glic::InvocationSource::kTopChromeButton);
}
#endif  // BUILDFLAG(ENABLE_GLIC)

void TabStripActionContainer::OnToggleActionUIState(const Browser* browser,
                                                    bool should_show) {
  CHECK(tab_organization_service_);

  if (locked_expansion_mode_ != LockedExpansionMode::kNone) {
    return;
  }

  if (should_show && browser_ == browser) {
    ShowTabStripNudge(auto_tab_group_button_);
  } else {
    HideTabStripNudge(auto_tab_group_button_);
  }
}

void TabStripActionContainer::OnTabDeclutterButtonClicked() {
  tabs::TabDeclutterController::EmitEntryPointHistogram(
      tab_search::mojom::TabDeclutterEntryPoint::kNudge);
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kAccepted);
  LogDeclutterTriggerBucket(true);

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabStripActionContainer::OnTabDeclutterButtonDismissed() {
  base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_declutter_controller_->OnActionUIDismissed(
      base::PassKey<TabStripActionContainer>());

  ExecuteHideTabStripNudge(tab_declutter_button_);
}

void TabStripActionContainer::OnGlicNudgeButtonClicked() {
  CHECK(glic_nudge_controller_);
  glic_nudge_controller_->OnNudgeActivity(
      tabs::GlicNudgeActivity::kNudgeClicked);
  tab_strip_controller_->GetBrowserWindowInterface()
      ->GetUserEducationInterface()
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGlicPromoFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_strip_controller_->GetProfile())
      ->ToggleUI(tab_strip_controller_->GetBrowserWindowInterface(),
                 /*prevent_close=*/true, glic::InvocationSource::kNudge);
#endif  // BUILDFLAG(ENABLE_GLIC)
  ExecuteHideTabStripNudge(glic_nudge_button_);
}

void TabStripActionContainer::OnGlicNudgeButtonDismissed() {
  glic_nudge_controller_->OnNudgeActivity(
      tabs::GlicNudgeActivity::kNudgeDismissed);

  ExecuteHideTabStripNudge(glic_nudge_button_);
}

void TabStripActionContainer::OnTriggerDeclutterUIVisibility() {
  CHECK(tab_declutter_controller_);

  ShowTabStripNudge(tab_declutter_button_);
}

void TabStripActionContainer::OnTriggerGlicNudgeUI(std::string label) {
  CHECK(glic_nudge_button_);
  glic_nudge_button_->SetText(base::UTF8ToUTF16(label));

  if (!label.empty()) {
    glic_nudge_controller_->OnNudgeActivity(
        tabs::GlicNudgeActivity::kNudgeShown);
    ShowTabStripNudge(glic_nudge_button_);
  } else {
    HideTabStripNudge(glic_nudge_button_);
  }
}

DeclutterTriggerCTRBucket TabStripActionContainer::GetDeclutterTriggerBucket(
    bool clicked) {
  const auto total_tab_count =
      tab_declutter_controller_->tab_strip_model()->GetTabCount();
  const auto stale_tab_count = tab_declutter_controller_->GetStaleTabs().size();

  if (total_tab_count < 15) {
    return clicked ? DeclutterTriggerCTRBucket::kClickedUnder15Tabs
                   : DeclutterTriggerCTRBucket::kShownUnder15Tabs;
  } else if (total_tab_count < 20) {
    if (stale_tab_count < 2) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19TabsUnder2Stale
                     : DeclutterTriggerCTRBucket::kShown15To19TabsUnder2Stale;
    } else if (stale_tab_count < 5) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19Tabs2To4Stale
                     : DeclutterTriggerCTRBucket::kShown15To19Tabs2To4Stale;
    } else if (stale_tab_count < 8) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19Tabs5To7Stale
                     : DeclutterTriggerCTRBucket::kShown15To19Tabs5To7Stale;
    } else {
      return clicked ? DeclutterTriggerCTRBucket::kClicked15To19TabsOver7Stale
                     : DeclutterTriggerCTRBucket::kShown15To19TabsOver7Stale;
    }
  } else if (total_tab_count < 25) {
    if (stale_tab_count < 2) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24TabsUnder2Stale
                     : DeclutterTriggerCTRBucket::kShown20To24TabsUnder2Stale;
    } else if (stale_tab_count < 5) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24Tabs2To4Stale
                     : DeclutterTriggerCTRBucket::kShown20To24Tabs2To4Stale;
    } else if (stale_tab_count < 8) {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24Tabs5To7Stale
                     : DeclutterTriggerCTRBucket::kShown20To24Tabs5To7Stale;
    } else {
      return clicked ? DeclutterTriggerCTRBucket::kClicked20To24TabsOver7Stale
                     : DeclutterTriggerCTRBucket::kShown20To24TabsOver7Stale;
    }
  } else {
    return clicked ? DeclutterTriggerCTRBucket::kClickedOver24Tabs
                   : DeclutterTriggerCTRBucket::kShownOver24Tabs;
  }
}

void TabStripActionContainer::LogDeclutterTriggerBucket(bool clicked) {
  const DeclutterTriggerCTRBucket bucket = GetDeclutterTriggerBucket(clicked);
  base::UmaHistogramEnumeration(kDeclutterTriggerBucketedCTRName, bucket);
}

void TabStripActionContainer::ShowTabStripNudge(TabStripNudgeButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillShow, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteShowTabStripNudge(button);
  }
}

void TabStripActionContainer::HideTabStripNudge(TabStripNudgeButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillHide, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteHideTabStripNudge(button);
  }
}

void TabStripActionContainer::ExecuteShowTabStripNudge(
    TabStripNudgeButton* button) {
  if (browser_ && (button == auto_tab_group_button_) &&
      !TabOrganizationUtils::GetInstance()->IsEnabled(browser_->profile())) {
    return;
  }

  // If the tab strip already has a modal UI showing, that is not for the same
  // button being hidden, exit early. If the tab strip has modal UI for the same
  // button being hidden, then continue to reset the animation and start a show
  // animation.
  if (!tab_strip_controller_->CanShowModalUI() &&
      !(animation_session_ &&
        animation_session_->session_type() ==
            TabStripNudgeAnimationSession::AnimationSessionType::HIDE &&
        animation_session_->button() == button)) {
    return;
  }

  if (button == glic_nudge_button_) {
#if BUILDFLAG(ENABLE_GLIC)
    // Hide the glic button while the nudge is shown
    if (glic_button_) {
      glic_button_->SetIsShowingNudge(true);
    }
    hide_tab_strip_nudge_timer_.Stop();
#endif  // BUILDFLAG(ENABLE_GLIC)
  } else {
    hide_tab_strip_nudge_timer_.Start(
        FROM_HERE, kShowDuration,
        base::BindOnce(&TabStripActionContainer::OnTabStripNudgeButtonTimeout,
                       base::Unretained(this), button));
  }

  scoped_tab_strip_modal_ui_.reset();
  scoped_tab_strip_modal_ui_ = tab_strip_controller_->ShowModalUI();

  animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
      button, this, TabStripNudgeAnimationSession::AnimationSessionType::SHOW,
      base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();

  if (button == tab_declutter_button_) {
    LogDeclutterTriggerBucket(false);
  }
}

void TabStripActionContainer::ExecuteHideTabStripNudge(
    TabStripNudgeButton* button) {
  // Hide the current animation if the shown button is the same button. Do not
  // create a new animation session.
  if (animation_session_ &&
      animation_session_->session_type() ==
          TabStripNudgeAnimationSession::AnimationSessionType::SHOW &&
      animation_session_->button() == button) {
    hide_tab_strip_nudge_timer_.Stop();
    animation_session_->Hide();
    return;
  }

  if (!button->GetVisible()) {
    return;
  }

  // Stop the timer since the chip might be getting hidden on user actions like
  // dismissal or click and not timeout.
  hide_tab_strip_nudge_timer_.Stop();
  animation_session_ = std::make_unique<TabStripNudgeAnimationSession>(
      button, this, TabStripNudgeAnimationSession::AnimationSessionType::HIDE,
      base::BindOnce(&TabStripActionContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();
}

void TabStripActionContainer::SetLockedExpansionMode(
    LockedExpansionMode mode,
    TabStripNudgeButton* button) {
  if (mode == LockedExpansionMode::kNone) {
    if (locked_expansion_mode_ == LockedExpansionMode::kWillShow) {
      ExecuteShowTabStripNudge(locked_expansion_button_);
    } else if (locked_expansion_mode_ == LockedExpansionMode::kWillHide) {
      ExecuteHideTabStripNudge(locked_expansion_button_);
    }
    locked_expansion_button_ = nullptr;
  } else {
    locked_expansion_button_ = button;
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void TabStripActionContainer::OnAutoTabGroupButtonClicked() {
  base::UmaHistogramEnumeration(kAutoTabGroupsTriggerOutcomeName,
                                TriggerOutcome::kAccepted);
  tab_organization_service_->OnActionUIAccepted(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", true);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabStripNudge(auto_tab_group_button_);
}

void TabStripActionContainer::OnAutoTabGroupButtonDismissed() {
  base::UmaHistogramEnumeration(kAutoTabGroupsTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_organization_service_->OnActionUIDismissed(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabStripNudge(auto_tab_group_button_);
}

void TabStripActionContainer::OnTabStripNudgeButtonTimeout(
    TabStripNudgeButton* button) {
  if (button == auto_tab_group_button_) {
    base::UmaHistogramEnumeration(kAutoTabGroupsTriggerOutcomeName,
                                  TriggerOutcome::kTimedOut);
    UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);
  } else if (button == tab_declutter_button_) {
    base::UmaHistogramEnumeration(kDeclutterTriggerOutcomeName,
                                  TriggerOutcome::kTimedOut);
  }

  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabStripNudge(button);
}

void TabStripActionContainer::SetupButtonProperties(
    TabStripNudgeButton* button) {
  // Set the margins for the button
  const int space_between_buttons = kLargeSpaceBetweenButtons;
  gfx::Insets margin;
  margin.set_right(space_between_buttons);
  button->SetProperty(views::kMarginsKey, margin);

  // Set opacity for the button
  button->SetOpacity(0);
}

void TabStripActionContainer::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);
}

void TabStripActionContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabStripActionContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
  animation_session_->MarkAnimationDone(animation);
}

void TabStripActionContainer::OnAnimationSessionEnded() {
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs.
  if (animation_session_->session_type() ==
      TabStripNudgeAnimationSession::AnimationSessionType::HIDE) {
    scoped_tab_strip_modal_ui_.reset();

#if BUILDFLAG(ENABLE_GLIC)
    if (glic_button_ && !glic_button_->GetVisible()) {
      // Show the glic button after the nudge is hidden
      glic_button_->SetIsShowingNudge(false);
    }
#endif  // BUILDFLAG(ENABLE_GLIC)
  }

  animation_session_.reset();
}

void TabStripActionContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
}

void TabStripActionContainer::UpdateButtonBorders(
    const gfx::Insets border_insets) {
  if (auto_tab_group_button_) {
    auto_tab_group_button_->SetBorder(views::CreateEmptyBorder(border_insets));
  }
  if (tab_declutter_button_) {
    tab_declutter_button_->SetBorder(views::CreateEmptyBorder(border_insets));
  }
  if (product_specifications_button_) {
    product_specifications_button_->SetBorder(
        views::CreateEmptyBorder(border_insets));
  }
  if (glic_nudge_button_) {
    glic_nudge_button_->SetBorder(views::CreateEmptyBorder(border_insets));
  }
}
BEGIN_METADATA(TabStripActionContainer)
END_METADATA
