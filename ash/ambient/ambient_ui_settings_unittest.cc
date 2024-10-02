// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_ui_settings.h"

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Eq;

}  // namespace

class AmbientUiSettingsTest : public ::testing::Test {
 protected:
  AmbientUiSettingsTest() {
    base::Value::Dict default_settings;
    default_settings.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
                         static_cast<int>(kDefaultAmbientTheme));
    test_pref_service_.registry()->RegisterDictionaryPref(
        ambient::prefs::kAmbientUiSettings, std::move(default_settings));
  }

  TestingPrefServiceSimple test_pref_service_;
};

TEST_F(AmbientUiSettingsTest, DefaultConstructor) {
  EXPECT_THAT(AmbientUiSettings().theme(), Eq(kDefaultAmbientTheme));
}

TEST_F(AmbientUiSettingsTest, PrefManagement) {
  AmbientUiSettings().WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));

  AmbientUiSettings(AmbientTheme::kFloatOnBy)
      .WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kFloatOnBy));

  AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds)
      .WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kVideo));
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).video(),
      Eq(AmbientVideo::kClouds));
}

TEST_F(AmbientUiSettingsTest, HandlesCorruptedPrefStorage) {
  {
    base::Value::Dict invalid_settings;
    invalid_settings.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
                         static_cast<int>(AmbientTheme::kMaxValue) + 1);
    test_pref_service_.SetDict(ambient::prefs::kAmbientUiSettings,
                               std::move(invalid_settings));
  }
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));
  {
    base::Value::Dict invalid_settings;
    invalid_settings.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
                         static_cast<int>(AmbientTheme::kVideo));
    invalid_settings.Set(ambient::prefs::kAmbientUiSettingsFieldVideo,
                         static_cast<int>(AmbientVideo::kMaxValue) + 1);
    test_pref_service_.SetDict(ambient::prefs::kAmbientUiSettings,
                               std::move(invalid_settings));
  }
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));

  AmbientUiSettings(AmbientTheme::kFloatOnBy)
      .WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kFloatOnBy));
}

TEST_F(AmbientUiSettingsTest, CrashesWithInvalidSettings) {
  EXPECT_DEATH_IF_SUPPORTED(AmbientUiSettings settings(AmbientTheme::kVideo),
                            "");
}

TEST_F(AmbientUiSettingsTest, ToString) {
  EXPECT_THAT(AmbientUiSettings().ToString(), Eq("SlideShow"));
  EXPECT_THAT(
      AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds).ToString(),
      Eq("Video.Clouds"));
}

}  // namespace ash
