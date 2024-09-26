// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"

#include <memory>
#include <vector>
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/test/ambient_ash_test_helper.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/web_applications/personalization_app/ambient_video_albums.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::IsSupersetOf;
using ::testing::IsTrue;
using ::testing::Pointee;

constexpr char kFakeTestEmail[] = "fakeemail@example.com";

class TestAmbientObserver
    : public ash::personalization_app::mojom::AmbientObserver {
 public:
  void OnAmbientModeEnabledChanged(bool ambient_mode_enabled) override {
    ambient_mode_enabled_ = ambient_mode_enabled;
  }

  void OnAnimationThemeChanged(ash::AmbientTheme animation_theme) override {
    animation_theme_ = animation_theme;
  }

  void OnTopicSourceChanged(ash::AmbientModeTopicSource topic_source) override {
    topic_source_ = topic_source;
  }

  void OnAlbumsChanged(
      std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums)
      override {
    albums_ = std::move(albums);
  }

  void OnScreenSaverDurationChanged(uint32_t minutes) override {
    duration_ = minutes;
  }

  void OnTemperatureUnitChanged(
      ash::AmbientModeTemperatureUnit temperature_unit) override {
    temperature_unit_ = temperature_unit;
  }

  void OnPreviewsFetched(const std::vector<GURL>& previews) override {
    previews_ = std::move(previews);
  }

  void OnAmbientUiVisibilityChanged(
      ash::AmbientUiVisibility visibility) override {
    ambient_ui_visibility_ = visibility;
  }

  mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
  pending_remote() {
    if (ambient_observer_receiver_.is_bound()) {
      ambient_observer_receiver_.reset();
    }

    return ambient_observer_receiver_.BindNewPipeAndPassRemote();
  }

  bool is_ambient_mode_enabled() {
    ambient_observer_receiver_.FlushForTesting();
    return ambient_mode_enabled_;
  }

  ash::AmbientTheme animation_theme() {
    ambient_observer_receiver_.FlushForTesting();
    return animation_theme_;
  }

  ash::AmbientModeTopicSource topic_source() {
    ambient_observer_receiver_.FlushForTesting();
    return topic_source_;
  }

  const std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr>&
  albums() {
    ambient_observer_receiver_.FlushForTesting();
    return albums_;
  }

  ash::AmbientModeTemperatureUnit temperature_unit() {
    ambient_observer_receiver_.FlushForTesting();
    return temperature_unit_;
  }

  ash::AmbientUiVisibility visibility() {
    ambient_observer_receiver_.FlushForTesting();
    return ambient_ui_visibility_;
  }

  std::vector<GURL> previews() {
    ambient_observer_receiver_.FlushForTesting();
    return previews_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::AmbientObserver>
      ambient_observer_receiver_{this};

  bool ambient_mode_enabled_ = false;

  ash::AmbientTheme animation_theme_ = ash::AmbientTheme::kSlideshow;
  uint32_t duration_ = 10;
  ash::AmbientModeTopicSource topic_source_ =
      ash::AmbientModeTopicSource::kArtGallery;
  ash::AmbientModeTemperatureUnit temperature_unit_ =
      ash::AmbientModeTemperatureUnit::kFahrenheit;
  ash::AmbientUiVisibility ambient_ui_visibility_ =
      ash::AmbientUiVisibility::kClosed;
  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums_;
  std::vector<GURL> previews_;
};

}  // namespace

class PersonalizationAppAmbientProviderImplTest : public ash::AshTestBase {
 public:
  PersonalizationAppAmbientProviderImplTest()
      : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kTimeOfDayWallpaper,
         ash::features::kTimeOfDayScreenSaver},
        {});
  }
  PersonalizationAppAmbientProviderImplTest(
      const PersonalizationAppAmbientProviderImplTest&) = delete;
  PersonalizationAppAmbientProviderImplTest& operator=(
      const PersonalizationAppAmbientProviderImplTest&) = delete;
  ~PersonalizationAppAmbientProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ash::AshTestBase::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    ambient_provider_ =
        std::make_unique<PersonalizationAppAmbientProviderImpl>(&web_ui_);

    ambient_provider_->BindInterface(
        ambient_provider_remote_.BindNewPipeAndPassReceiver());

    SetEnabledPref(true);
    GetAmbientAshTestHelper()->ambient_client().SetAutomaticalyIssueToken(true);

    Shell::Get()->ambient_controller()->set_backend_controller_for_testing(
        nullptr);

    fake_backend_controller_ =
        std::make_unique<ash::FakeAmbientBackendControllerImpl>();
  }

  void TearDown() override {
    // The PersonalizationAppAmbientProviderImpl holds a pointer to the
    // AmbientController the Shell owns (which is destructed in
    // AshTestBase::Teardown), so reset it first.
    ambient_provider_.reset();
    ash::AshTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::AmbientProvider>&
  ambient_provider_remote() {
    return ambient_provider_remote_;
  }

  content::TestWebUI* web_ui() { return &web_ui_; }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void SetAmbientObserver() {
    ambient_provider_remote_->SetAmbientObserver(
        test_ambient_observer_.pending_remote());
  }

  bool ObservedAmbientModeEnabled() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.is_ambient_mode_enabled();
  }

  ash::AmbientTheme ObservedAnimationTheme() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.animation_theme();
  }

  ash::AmbientModeTopicSource ObservedTopicSource() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.topic_source();
  }

  const std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr>&
  ObservedAlbums() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.albums();
  }

  ash::AmbientModeTemperatureUnit ObservedTemperatureUnit() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.temperature_unit();
  }

  ash::AmbientUiVisibility ObservedAmbientUiVisibility() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.visibility();
  }

  std::vector<GURL> ObservedPreviews() {
    ambient_provider_remote_.FlushForTesting();
    return test_ambient_observer_.previews();
  }

  absl::optional<ash::AmbientSettings>& settings() {
    return ambient_provider_->settings_;
  }

  void SetEnabledPref(bool enabled) {
    profile()->GetPrefs()->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled,
                                      enabled);
  }

  void SetAnimationTheme(ash::AmbientTheme animation_theme) {
    ambient_provider_->SetAnimationTheme(animation_theme);
  }

  void FetchSettings() {
    ambient_provider_remote()->FetchSettingsAndAlbums();
    ambient_provider_remote().FlushForTesting();
  }

  void UpdateSettings() {
    if (!ambient_provider_->settings_)
      ambient_provider_->settings_ = ash::AmbientSettings();

    ambient_provider_->UpdateSettings();
  }

  void SetScreenSaverDuration(int minutes) {
    ambient_provider_->SetScreenSaverDuration(minutes);
  }

  void SetTopicSource(ash::AmbientModeTopicSource topic_source) {
    ambient_provider_->SetTopicSource(topic_source);
  }

  void SetAlbumSelected(const std::string& id,
                        ash::AmbientModeTopicSource topic_source,
                        bool selected) {
    ambient_provider_->SetAlbumSelected(id, topic_source, selected);
  }

  void FetchPreviewImages() { ambient_provider_->FetchPreviewImages(); }

  ash::AmbientModeTopicSource TopicSource() {
    return ambient_provider_->settings_->topic_source;
  }

  std::vector<std::string> SelectedAlbumIds() {
    return ambient_provider_->settings_->selected_album_ids;
  }

  void SetSelectedAlbumIds(const std::vector<std::string>& ids) {
    ambient_provider_->settings_->selected_album_ids = ids;
  }

  void SetTemperatureUnit(ash::AmbientModeTemperatureUnit temperature_unit) {
    ambient_provider_->SetTemperatureUnit(temperature_unit);
  }

  ash::AmbientModeTemperatureUnit TemperatureUnit() {
    return ambient_provider_->settings_->temperature_unit;
  }

  std::vector<ash::ArtSetting> ArtSettings() {
    return ambient_provider_->settings_->art_settings;
  }

  bool HasPendingFetchRequestAtProvider() const {
    return ambient_provider_->has_pending_fetch_request_;
  }

  bool IsUpdateSettingsPendingAtProvider() const {
    return ambient_provider_->is_updating_backend_;
  }

  bool HasPendingUpdatesAtProvider() const {
    return ambient_provider_->has_pending_updates_for_backend_;
  }

  base::TimeDelta GetFetchSettingsDelay() {
    return ambient_provider_->fetch_settings_retry_backoff_
        .GetTimeUntilRelease();
  }

  base::TimeDelta GetUpdateSettingsDelay() {
    return ambient_provider_->update_settings_retry_backoff_
        .GetTimeUntilRelease();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment()->FastForwardBy(time);
  }

  bool IsFetchSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsFetchSettingsAndAlbumsPending();
  }

  void ReplyFetchSettingsAndAlbums(
      bool success,
      absl::optional<ash::AmbientSettings> settings = absl::nullopt) {
    fake_backend_controller_->ReplyFetchSettingsAndAlbums(success,
                                                          std::move(settings));
  }

  bool IsUpdateSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsUpdateSettingsPending();
  }

  void ReplyUpdateSettings(bool success) {
    fake_backend_controller_->ReplyUpdateSettings(success);
  }

  void EnableUpdateSettingsAutoReply(bool success) {
    fake_backend_controller_->EnableUpdateSettingsAutoReply(success);
  }

  AmbientModeTemperatureUnit GetCurrentTemperatureUnitInServer() const {
    return fake_backend_controller_->current_temperature_unit();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<TestingProfile, ExperimentalAsh> profile_;
  mojo::Remote<ash::personalization_app::mojom::AmbientProvider>
      ambient_provider_remote_;
  std::unique_ptr<PersonalizationAppAmbientProviderImpl> ambient_provider_;
  TestAmbientObserver test_ambient_observer_;

  std::unique_ptr<ash::FakeAmbientBackendControllerImpl>
      fake_backend_controller_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PersonalizationAppAmbientProviderImplTest, IsAmbientModeEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, true);
  bool called = false;
  ambient_provider_remote()->IsAmbientModeEnabled(
      base::BindLambdaForTesting([&called](bool enabled) {
        called = true;
        EXPECT_TRUE(enabled);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);

  called = false;
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);
  ambient_provider_remote()->IsAmbientModeEnabled(
      base::BindLambdaForTesting([&called](bool enabled) {
        called = true;
        EXPECT_FALSE(enabled);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetAmbientModeEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  // Clear pref.
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);

  ambient_provider_remote()->SetAmbientModeEnabled(true);
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));

  ambient_provider_remote()->SetAmbientModeEnabled(false);
  ambient_provider_remote().FlushForTesting();
  EXPECT_FALSE(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAmbientModeEnabledChanged) {
  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_TRUE(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, false);
  SetAmbientObserver();
  FetchSettings();
  EXPECT_FALSE(ObservedAmbientModeEnabled());

  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, true);
  SetAmbientObserver();
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(ObservedAmbientModeEnabled());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAnimationThemeChanged) {
  SetAmbientObserver();
  FetchSettings();
  SetAnimationTheme(ash::AmbientTheme::kSlideshow);
  EXPECT_EQ(ash::AmbientTheme::kSlideshow, ObservedAnimationTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       ash::AmbientTheme::kSlideshow, 1);

  SetAnimationTheme(ash::AmbientTheme::kFeelTheBreeze);
  EXPECT_EQ(ash::AmbientTheme::kFeelTheBreeze, ObservedAnimationTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       ash::AmbientTheme::kFeelTheBreeze, 1);

  SetAnimationTheme(ash::AmbientTheme::kVideo);
  EXPECT_EQ(ash::AmbientTheme::kVideo, ObservedAnimationTheme());
  histogram_tester().ExpectBucketCount(kAmbientModeAnimationThemeHistogramName,
                                       ash::AmbientTheme::kVideo, 1);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, FetchPreviewImages) {
  SetAmbientObserver();
  EXPECT_TRUE(ObservedPreviews().empty());
  FetchPreviewImages();
  EXPECT_FALSE(ObservedPreviews().empty());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnTopicSourceChanged) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, ObservedTopicSource());
  EXPECT_FALSE(ObservedPreviews().empty());

  SetTopicSource(ash::AmbientModeTopicSource::kArtGallery);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, ObservedTopicSource());

  // The `kVideo` topic source is exclusive to the `kVideo` theme. It does not
  // apply to any of the other themes, so the existing topic source sticks.
  SetTopicSource(ash::AmbientModeTopicSource::kVideo);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, ObservedTopicSource());

  SetAnimationTheme(AmbientTheme::kVideo);
  EXPECT_EQ(AmbientModeTopicSource::kVideo, ObservedTopicSource());

  // The other topic sources do not apply to the video theme, so all other
  // `SeTopicSource()` calls should be rejected.
  SetTopicSource(AmbientModeTopicSource::kArtGallery);
  EXPECT_EQ(AmbientModeTopicSource::kVideo, ObservedTopicSource());
  SetTopicSource(AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(AmbientModeTopicSource::kVideo, ObservedTopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, ShouldCallOnAlbumsChanged) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // The fake albums are set in FakeAmbientBackendControllerImpl. Hidden setting
  // will be sent to JS side.
  EXPECT_EQ(6u, ObservedAlbums().size());
  EXPECT_FALSE(ObservedPreviews().empty());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnTemperatureUnitChanged) {
  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kFahrenheit);
  ReplyUpdateSettings(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit,
            GetCurrentTemperatureUnitInServer());

  // Even while the video topic source is active, temperature settings changes
  // should still be sent to the backend.
  SetAnimationTheme(AmbientTheme::kVideo);
  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);
  ReplyUpdateSettings(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            ObservedTemperatureUnit());
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius,
            GetCurrentTemperatureUnitInServer());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       ShouldCallOnAmbientUiVisibilityChanged) {
  SetAmbientObserver();
  EXPECT_EQ(ash::AmbientUiVisibility::kClosed, ObservedAmbientUiVisibility());
  Shell::Get()->ambient_controller()->ambient_ui_model()->SetUiVisibility(
      ash::AmbientUiVisibility::kPreview);
  EXPECT_EQ(ash::AmbientUiVisibility::kPreview, ObservedAmbientUiVisibility());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetTopicSource) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());

  SetTopicSource(ash::AmbientModeTopicSource::kArtGallery);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, TopicSource());

  SetTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());

  // If `settings_->selected_album_ids` is empty, will fallback to kArtGallery.
  SetSelectedAlbumIds(/*ids=*/{});
  SetTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, TopicSource());

  SetSelectedAlbumIds(/*ids=*/{"1"});
  SetTopicSource(ash::AmbientModeTopicSource::kGooglePhotos);
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, SetTemperatureUnit) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius, TemperatureUnit());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kFahrenheit);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kFahrenheit, TemperatureUnit());

  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);
  EXPECT_EQ(ash::AmbientModeTemperatureUnit::kCelsius, TemperatureUnit());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestFetchSettings) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsFailedWillRetry) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsSecondRetryWillBackoff) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay1 = GetFetchSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay2 = GetFetchSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestFetchSettingsWillNotRetryMoreThanThreeTimes) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 1st retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 2nd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 3rd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // Will not retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestUpdateSettings) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestUpdateSettingsTwice) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(HasPendingUpdatesAtProvider());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsFailedWillRetry) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsSecondRetryWillBackoff) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  base::TimeDelta delay1 = GetUpdateSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  base::TimeDelta delay2 = GetUpdateSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestUpdateSettingsWillNotRetryMoreThanThreeTimes) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());

  // Will not retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(HasPendingUpdatesAtProvider());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestNoFetchRequestWhenUpdatingSettings) {
  EXPECT_FALSE(HasPendingFetchRequestAtProvider());
  UpdateSettings();
  EXPECT_FALSE(HasPendingFetchRequestAtProvider());

  FetchSettings();
  EXPECT_TRUE(HasPendingFetchRequestAtProvider());
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestSetSelectedGooglePhotosAlbum) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  // The fake data has album '1' as selected.
  std::vector<std::string> selected_ids = SelectedAlbumIds();
  EXPECT_TRUE(base::Contains(selected_ids, "1"));

  ash::personalization_app::mojom::AmbientModeAlbumPtr album =
      ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '1';
  album->topic_source = ash::AmbientModeTopicSource::kGooglePhotos;
  album->checked = false;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  selected_ids = SelectedAlbumIds();
  EXPECT_TRUE(selected_ids.empty());
  // Will fallback to Art topic source if no selected Google Photos.
  EXPECT_EQ(ash::AmbientModeTopicSource::kArtGallery, TopicSource());

  album = ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '1';
  album->topic_source = ash::AmbientModeTopicSource::kGooglePhotos;
  album->checked = true;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  selected_ids = SelectedAlbumIds();
  EXPECT_EQ(1u, selected_ids.size());
  EXPECT_TRUE(base::Contains(selected_ids, "1"));
  EXPECT_EQ(ash::AmbientModeTopicSource::kGooglePhotos, TopicSource());
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestSetSelectedArtAlbum) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  // The fake data has art setting '0' as enabled.
  std::vector<ash::ArtSetting> art_settings = ArtSettings();
  auto it = base::ranges::find_if(art_settings, &ash::ArtSetting::enabled);
  EXPECT_NE(it, art_settings.end());
  EXPECT_EQ(it->album_id, "0");

  ash::personalization_app::mojom::AmbientModeAlbumPtr album =
      ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '0';
  album->topic_source = ash::AmbientModeTopicSource::kArtGallery;
  album->checked = false;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  art_settings = ArtSettings();
  EXPECT_TRUE(base::ranges::none_of(art_settings, &ash::ArtSetting::enabled));

  album = ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '1';
  album->topic_source = ash::AmbientModeTopicSource::kArtGallery;
  album->checked = true;
  SetAlbumSelected(album->id, album->topic_source, album->checked);

  art_settings = ArtSettings();
  it = base::ranges::find_if(art_settings, &ash::ArtSetting::enabled);
  EXPECT_NE(it, art_settings.end());
  EXPECT_EQ(it->album_id, "1");
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestSetSelectedVideo) {
  auto expect_videos_selected = [this](bool clouds_selected,
                                       bool new_mexico_select) {
    EXPECT_THAT(
        ObservedAlbums(),
        IsSupersetOf({Pointee(AllOf(Field(&mojom::AmbientModeAlbum::id,
                                          Eq(kCloudsAlbumId)),
                                    Field(&mojom::AmbientModeAlbum::checked,
                                          Eq(clouds_selected)))),
                      Pointee(AllOf(Field(&mojom::AmbientModeAlbum::id,
                                          Eq(kNewMexicoAlbumId)),
                                    Field(&mojom::AmbientModeAlbum::checked,
                                          Eq(new_mexico_select))))}));
  };

  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  // Even before the video theme is selected, the default video should be
  // present in the list of albums and considered "checked".
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);

  SetAnimationTheme(AmbientTheme::kVideo);

  // After video theme is selected, the default video should remain checked.
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);

  // Switch to clouds.
  SetAlbumSelected(kCloudsAlbumId.data(), AmbientModeTopicSource::kVideo, true);
  expect_videos_selected(/*clouds_selected=*/true,
                         /*new_mexico_selected=*/false);

  // Switch back to new mexico.
  SetAlbumSelected(kNewMexicoAlbumId.data(), AmbientModeTopicSource::kVideo,
                   true);
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);

  // Should never be in a state where there are no videos selected.
  SetAlbumSelected(kNewMexicoAlbumId.data(), AmbientModeTopicSource::kVideo,
                   false);
  expect_videos_selected(/*clouds_selected=*/false,
                         /*new_mexico_selected=*/true);
}

TEST_F(PersonalizationAppAmbientProviderImplTest, TestAlbumNumbersAreRecorded) {
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  ash::personalization_app::mojom::AmbientModeAlbumPtr album =
      ash::personalization_app::mojom::AmbientModeAlbum::New();
  album->id = '0';
  album->topic_source = ash::AmbientModeTopicSource::kGooglePhotos;
  SetAlbumSelected(album->id, album->topic_source, album->checked);
  histogram_tester().ExpectTotalCount("Ash.AmbientMode.TotalNumberOfAlbums",
                                      /*count=*/1);
  histogram_tester().ExpectTotalCount("Ash.AmbientMode.SelectedNumberOfAlbums",
                                      /*count=*/1);
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestEnabledPrefChangeUpdatesSettings) {
  // Simulate initial page request.
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // Should not trigger |UpdateSettings|.
  SetEnabledPref(/*enabled=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // Settings this to true should trigger |UpdateSettings|.
  SetEnabledPref(/*enabled=*/true);
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       TestWeatherFalseTriggersUpdateSettings) {
  ash::AmbientSettings weather_off_settings;
  weather_off_settings.show_weather = false;

  // Simulate initial page request with weather settings false. Because Ambient
  // mode pref is enabled and |settings.show_weather| is false, this should
  // trigger a call to |UpdateSettings| that sets |settings.show_weather| to
  // true.
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true, weather_off_settings);

  // A call to |UpdateSettings| should have happened.
  EXPECT_TRUE(IsUpdateSettingsPendingAtProvider());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtProvider());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // |settings.show_weather| should now be true after the successful settings
  // update.
  EXPECT_TRUE(settings()->show_weather);
}

// b/236723933
TEST_F(PersonalizationAppAmbientProviderImplTest,
       DoesNotCrashWithEmptyGooglePhotosAlbums) {
  SetEnabledPref(/*enabled=*/false);
  FetchSettings();
  // Reply with settings with |kGooglePhotos| but empty |selected_album_ids|.
  ash::AmbientSettings settings;
  settings.topic_source = AmbientModeTopicSource::kGooglePhotos;
  ReplyFetchSettingsAndAlbums(/*success=*/true,
                              /*settings=*/std::move(settings));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       HandlesTransitionToFromVideoTopicSource) {
  // Start with the video topic source already active on boot.
  AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds)
      .WriteToPrefService(*profile()->GetPrefs());

  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  EXPECT_EQ(ObservedTopicSource(), AmbientModeTopicSource::kVideo);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(
                  AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kCloudsAlbumId)),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));

  // Switch to slide show mode and change settings to some custom configuration.
  SetAnimationTheme(AmbientTheme::kSlideshow);
  SetTopicSource(AmbientModeTopicSource::kArtGallery);
  SetAlbumSelected("1", AmbientModeTopicSource::kArtGallery, /*selected=*/true);
  ReplyUpdateSettings(/*success=*/true);

  // Switch back to video theme. Same video settings should remain.
  SetAnimationTheme(AmbientTheme::kVideo);
  EXPECT_EQ(ObservedTopicSource(), AmbientModeTopicSource::kVideo);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(
                  AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kCloudsAlbumId)),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));

  // Switch back to slide show. The custom setting set previously should stick.
  SetAnimationTheme(AmbientTheme::kSlideshow);
  EXPECT_EQ(ObservedTopicSource(), AmbientModeTopicSource::kArtGallery);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(
                  AllOf(Field(&mojom::AmbientModeAlbum::id, Eq("1")),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       HandlesFailedSettingsUpdateForVideo) {
  EnableUpdateSettingsAutoReply(/*success=*/false);

  SetAmbientObserver();
  FetchSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  SetAnimationTheme(AmbientTheme::kVideo);
  // Let retries happen and try to expose any erroneous settings changes.
  task_environment()->FastForwardBy(base::Minutes(1));
  // Should not get stuck in a state where video theme is active with a
  // non-video topic source.
  ASSERT_EQ(ObservedAnimationTheme(), AmbientTheme::kVideo);
  EXPECT_EQ(ObservedTopicSource(), AmbientModeTopicSource::kVideo);
  EXPECT_THAT(ObservedAlbums(),
              Contains(Pointee(AllOf(
                  Field(&mojom::AmbientModeAlbum::id, Eq(kNewMexicoAlbumId)),
                  Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));
}

TEST_F(PersonalizationAppAmbientProviderImplTest,
       DismissingBannerHidesItForever) {
  bool called = false;
  ambient_provider_remote()->ShouldShowTimeOfDayBanner(
      base::BindLambdaForTesting([&called](bool should_show_banner) {
        called = true;
        EXPECT_TRUE(should_show_banner);
      }));
  ambient_provider_remote().FlushForTesting();
  EXPECT_TRUE(called);

  ambient_provider_remote()->HandleTimeOfDayBannerDismissed();

  ambient_provider_remote()->ShouldShowTimeOfDayBanner(
      base::BindLambdaForTesting(
          [](bool should_show_nudge) { EXPECT_FALSE(should_show_nudge); }));
  ambient_provider_remote().FlushForTesting();
}

}  // namespace ash::personalization_app
