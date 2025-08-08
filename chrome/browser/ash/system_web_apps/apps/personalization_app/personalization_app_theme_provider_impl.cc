// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_theme_provider_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/shell.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/color_util.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"

namespace ash::personalization_app {

// This array represents the order, number, and types of color schemes
// represented by the color scheme buttons in the app.
const std::array<ash::style::mojom::ColorScheme, 4> kColorSchemeButtons{
    ash::style::mojom::ColorScheme::kTonalSpot,
    ash::style::mojom::ColorScheme::kNeutral,
    ash::style::mojom::ColorScheme::kVibrant,
    ash::style::mojom::ColorScheme::kExpressive,
};

PersonalizationAppThemeProviderImpl::PersonalizationAppThemeProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  if (chromeos::features::IsJellyEnabled()) {
    color_palette_controller_ = Shell::Get()->color_palette_controller();
  }
}

PersonalizationAppThemeProviderImpl::~PersonalizationAppThemeProviderImpl() =
    default;

void PersonalizationAppThemeProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
        receiver) {
  theme_receiver_.reset();
  theme_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppThemeProviderImpl::SetThemeObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
        observer) {
  // May already be bound if user refreshes page.
  theme_observer_remote_.reset();
  theme_observer_remote_.Bind(std::move(observer));
  if (!color_mode_observer_.IsObserving()) {
    color_mode_observer_.Observe(ash::DarkLightModeControllerImpl::Get());
  }
  // Call it once to get the current color mode.
  OnColorModeChanged(
      ash::DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  // Listen to |ash::prefs::kDarkModeScheduleType| changes.
  if (!pref_change_registrar_.IsObserved(ash::prefs::kDarkModeScheduleType)) {
    pref_change_registrar_.Add(
        ash::prefs::kDarkModeScheduleType,
        base::BindRepeating(&PersonalizationAppThemeProviderImpl::
                                NotifyColorModeAutoScheduleChanged,
                            base::Unretained(this)));
  }
  // Call once to get the initial status.
  NotifyColorModeAutoScheduleChanged();
  if (chromeos::features::IsJellyEnabled()) {
    OnStaticColorChanged();
    OnColorSchemeChanged();
    if (!pref_change_registrar_.IsObserved(
            ash::prefs::kDynamicColorColorScheme)) {
      pref_change_registrar_.Add(
          ash::prefs::kDynamicColorColorScheme,
          base::BindRepeating(
              &PersonalizationAppThemeProviderImpl::OnColorSchemeChanged,
              base::Unretained(this)));
    }
    if (!pref_change_registrar_.IsObserved(
            ash::prefs::kDynamicColorSeedColor)) {
      pref_change_registrar_.Add(
          ash::prefs::kDynamicColorSeedColor,
          base::BindRepeating(
              &PersonalizationAppThemeProviderImpl::OnStaticColorChanged,
              base::Unretained(this)));
    }
    ui::ColorProviderSourceObserver::Observe(
        ash::ColorUtil::GetColorProviderSourceForWindow(
            ash::Shell::GetPrimaryRootWindow()));
  }
}

void PersonalizationAppThemeProviderImpl::SetColorModePref(
    bool dark_mode_enabled) {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  if (dark_light_mode_controller->IsDarkModeEnabled() != dark_mode_enabled) {
    LogPersonalizationTheme(dark_mode_enabled ? ColorMode::kDark
                                              : ColorMode::kLight);
    dark_light_mode_controller->ToggleColorMode();
  }
}

void PersonalizationAppThemeProviderImpl::SetColorModeAutoScheduleEnabled(
    bool enabled) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  if (enabled) {
    LogPersonalizationTheme(ColorMode::kAuto);
  }
  const ScheduleType schedule_type =
      enabled ? ScheduleType::kSunsetToSunrise : ScheduleType::kNone;
  pref_service->SetInteger(ash::prefs::kDarkModeScheduleType,
                           static_cast<int>(schedule_type));
}

void PersonalizationAppThemeProviderImpl::IsDarkModeEnabled(
    IsDarkModeEnabledCallback callback) {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  std::move(callback).Run(dark_light_mode_controller->IsDarkModeEnabled());
}

void PersonalizationAppThemeProviderImpl::IsColorModeAutoScheduleEnabled(
    IsColorModeAutoScheduleEnabledCallback callback) {
  std::move(callback).Run(IsColorModeAutoScheduleEnabled());
}

void PersonalizationAppThemeProviderImpl::OnColorModeChanged(
    bool dark_mode_enabled) {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorModeChanged(dark_mode_enabled);
}

void PersonalizationAppThemeProviderImpl::OnColorSchemeChanged() {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorSchemeChanged(
      color_palette_controller_->GetColorScheme(GetAccountId(profile_)));
}

void PersonalizationAppThemeProviderImpl::OnStaticColorChanged() {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnStaticColorChanged(
      color_palette_controller_->GetStaticColor(GetAccountId(profile_)));
}

void PersonalizationAppThemeProviderImpl::OnSampleColorSchemesChanged(
    const std::vector<ash::SampleColorScheme>& sampleColorSchemes) {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnSampleColorSchemesChanged(sampleColorSchemes);
}

bool PersonalizationAppThemeProviderImpl::IsColorModeAutoScheduleEnabled() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  const auto schedule_type = static_cast<ScheduleType>(
      pref_service->GetInteger(ash::prefs::kDarkModeScheduleType));
  return schedule_type == ScheduleType::kSunsetToSunrise;
}

void PersonalizationAppThemeProviderImpl::NotifyColorModeAutoScheduleChanged() {
  DCHECK(theme_observer_remote_.is_bound());
  theme_observer_remote_->OnColorModeAutoScheduleChanged(
      IsColorModeAutoScheduleEnabled());
}

void PersonalizationAppThemeProviderImpl::GetColorScheme(
    GetColorSchemeCallback callback) {
  if (!chromeos::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call GetColorScheme without Jelly enabled.");
    return;
  }
  std::move(callback).Run(
      color_palette_controller_->GetColorScheme(GetAccountId(profile_)));
}

void PersonalizationAppThemeProviderImpl::SetColorScheme(
    ash::style::mojom::ColorScheme color_scheme) {
  if (!chromeos::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call SetColorScheme without Jelly enabled.");
    return;
  }
  color_palette_controller_->SetColorScheme(
      color_scheme, GetAccountId(profile_), base::DoNothing());
}

void PersonalizationAppThemeProviderImpl::GetStaticColor(
    GetStaticColorCallback callback) {
  if (!chromeos::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call GetStaticColor without Jelly enabled.");
    return;
  }
  std::move(callback).Run(
      color_palette_controller_->GetStaticColor(GetAccountId(profile_)));
}

void PersonalizationAppThemeProviderImpl::SetStaticColor(SkColor static_color) {
  if (!chromeos::features::IsJellyEnabled()) {
    theme_receiver_.ReportBadMessage(
        "Cannot call SetStaticColor without Jelly enabled.");
    return;
  }
  AccountId account_id = GetAccountId(profile_);
  color_palette_controller_->SetStaticColor(static_color, account_id,
                                            base::DoNothing());
}

void PersonalizationAppThemeProviderImpl::GenerateSampleColorSchemes(
    GenerateSampleColorSchemesCallback callback) {
  color_palette_controller_->GenerateSampleColorSchemes(kColorSchemeButtons,
                                                        std::move(callback));
}

void PersonalizationAppThemeProviderImpl::OnColorProviderChanged() {
  GenerateSampleColorSchemes(base::BindOnce(
      &PersonalizationAppThemeProviderImpl::OnSampleColorSchemesChanged,
      weak_factory_.GetWeakPtr()));
}
}  // namespace ash::personalization_app
