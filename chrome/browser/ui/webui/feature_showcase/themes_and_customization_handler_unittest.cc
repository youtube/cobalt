// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/themes_and_customization_handler.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ThemesAndCustomizationHandlerTest : public testing::Test {
 protected:
  TestingProfile& profile() { return profile_; }

  ThemeService& theme_service() {
    return CHECK_DEREF(ThemeServiceFactory::GetForProfile(&profile()));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ThemesAndCustomizationHandlerTest,
       RevertThemeRecoversOriginalColorScheme) {
  theme_service().SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  theme_service().SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);

  mojo::PendingReceiver<
      feature_showcase::mojom::ThemesAndCustomizationPageHandler>
      receiver;
  auto handler = std::make_unique<ThemesAndCustomizationHandler>(
      std::move(receiver), &theme_service());
  handler->SnapshotTheme();

  theme_service().SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kDark);
  theme_service().SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);

  ASSERT_EQ(ThemeService::BrowserColorScheme::kDark,
            theme_service().GetBrowserColorScheme());
  ASSERT_THAT(theme_service().GetUserColor(), testing::Optional(SK_ColorBLUE));

  handler->RevertTheme();

  ASSERT_EQ(ThemeService::BrowserColorScheme::kLight,
            theme_service().GetBrowserColorScheme());
  ASSERT_THAT(theme_service().GetUserColor(), testing::Optional(SK_ColorRED));
}

TEST_F(ThemesAndCustomizationHandlerTest,
       DestructorRecoversOriginalColorScheme) {
  theme_service().SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  theme_service().SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);

  mojo::PendingReceiver<
      feature_showcase::mojom::ThemesAndCustomizationPageHandler>
      receiver;
  auto handler = std::make_unique<ThemesAndCustomizationHandler>(
      std::move(receiver), &theme_service());
  handler->SnapshotTheme();

  theme_service().SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kDark);
  theme_service().SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);

  ASSERT_EQ(ThemeService::BrowserColorScheme::kDark,
            theme_service().GetBrowserColorScheme());
  ASSERT_THAT(theme_service().GetUserColor(), testing::Optional(SK_ColorBLUE));

  // Handler should revert the theme when destroyed without 'Accept theme' click
  handler.reset();

  ASSERT_EQ(ThemeService::BrowserColorScheme::kLight,
            theme_service().GetBrowserColorScheme());
  ASSERT_THAT(theme_service().GetUserColor(), testing::Optional(SK_ColorRED));
}

TEST_F(ThemesAndCustomizationHandlerTest, AcceptThemePreventsRevert) {
  theme_service().SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  theme_service().SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kTonalSpot);

  mojo::PendingReceiver<
      feature_showcase::mojom::ThemesAndCustomizationPageHandler>
      receiver;
  auto handler = std::make_unique<ThemesAndCustomizationHandler>(
      std::move(receiver), &theme_service());
  handler->SnapshotTheme();

  theme_service().SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kDark);
  theme_service().SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);

  ASSERT_EQ(ThemeService::BrowserColorScheme::kDark,
            theme_service().GetBrowserColorScheme());
  ASSERT_THAT(theme_service().GetUserColor(), testing::Optional(SK_ColorBLUE));

  handler->AcceptTheme();

  // Handler should not revert the theme.
  handler.reset();

  ASSERT_EQ(ThemeService::BrowserColorScheme::kDark,
            theme_service().GetBrowserColorScheme());
  ASSERT_THAT(theme_service().GetUserColor(), testing::Optional(SK_ColorBLUE));
}

}  // namespace
