// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/personalization_app_mojom_banned_mocha_test_base.h"
#include "content/public/test/browser_test.h"

namespace ash::personalization_app {

// Tests individual Polymer element files used in chrome://personalization.
using PersonalizationAppComponentTest =
    PersonalizationAppMojomBannedMochaTestBase;

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, AmbientObserver) {
  RunTest("chromeos/personalization_app/ambient_observer_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, AmbientPreviewLarge) {
  RunTest("chromeos/personalization_app/ambient_preview_large_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, AmbientPreviewSmall) {
  RunTest("chromeos/personalization_app/ambient_preview_small_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, AmbientSubpage) {
  RunTest("chromeos/personalization_app/ambient_subpage_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, AvatarCamera) {
  RunTest("chromeos/personalization_app/avatar_camera_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, AvatarList) {
  RunTest("chromeos/personalization_app/avatar_list_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, DynamicColor) {
  RunTest("chromeos/personalization_app/dynamic_color_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, GooglePhotosAlbums) {
  RunTest("chromeos/personalization_app/google_photos_albums_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest,
                       GooglePhotosCollection) {
  RunTest(
      "chromeos/personalization_app/google_photos_collection_element_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest,
                       GooglePhotosPhotosByAlbumId) {
  RunTest(
      "chromeos/personalization_app/"
      "google_photos_photos_by_album_id_element_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, GooglePhotosPhotos) {
  RunTest("chromeos/personalization_app/google_photos_photos_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest,
                       GooglePhotosSharedAlbumDialog) {
  RunTest(
      "chromeos/personalization_app/"
      "google_photos_shared_album_dialog_element_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, GooglePhotosZeroState) {
  RunTest(
      "chromeos/personalization_app/google_photos_zero_state_element_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, KeyboardBacklight) {
  RunTest("chromeos/personalization_app/keyboard_backlight_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, LocalImages) {
  RunTest("chromeos/personalization_app/local_images_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest,
                       PersonalizationBreadcrumb) {
  RunTest(
      "chromeos/personalization_app/personalization_breadcrumb_element_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, PersonalizationMain) {
  RunTest("chromeos/personalization_app/personalization_main_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, PersonalizationRouter) {
  RunTest("chromeos/personalization_app/personalization_router_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, PersonalizationTheme) {
  RunTest("chromeos/personalization_app/personalization_theme_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, PersonalizationToast) {
  RunTest("chromeos/personalization_app/personalization_toast_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, SeaPenCollection) {
  RunTest("chromeos/personalization_app/sea_pen_collection_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, SeaPenImages) {
  RunTest("chromeos/personalization_app/sea_pen_images_element_test.js",
          "mocha.run()");
}


IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, UserPreview) {
  RunTest("chromeos/personalization_app/user_preview_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, UserSubpage) {
  RunTest("chromeos/personalization_app/user_subpage_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperCollections) {
  RunTest("chromeos/personalization_app/wallpaper_collections_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperFullscreen) {
  RunTest("chromeos/personalization_app/wallpaper_fullscreen_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperGridItem) {
  RunTest("chromeos/personalization_app/wallpaper_grid_item_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperImages) {
  RunTest("chromeos/personalization_app/wallpaper_images_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperObserver) {
  RunTest("chromeos/personalization_app/wallpaper_observer_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperPreview) {
  RunTest("chromeos/personalization_app/wallpaper_preview_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperSelected) {
  RunTest("chromeos/personalization_app/wallpaper_selected_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperSubpage) {
  RunTest("chromeos/personalization_app/wallpaper_subpage_element_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, WallpaperSubpageTop) {
  RunTest("chromeos/personalization_app/wallpaper_subpage_top_element_test.js",
          "mocha.run()");
}
IN_PROC_BROWSER_TEST_F(PersonalizationAppComponentTest, ZoneCustomization) {
  RunTest("chromeos/personalization_app/zone_customization_element_test.js",
          "mocha.run()");
}

// Tests state management and logic in chrome://personalization.
using PersonalizationAppControllerTest =
    PersonalizationAppMojomBannedMochaTestBase;

IN_PROC_BROWSER_TEST_F(PersonalizationAppControllerTest, All) {
  RunTest("chromeos/personalization_app/personalization_app_controller_test.js",
          "mocha.run()");
}

}  // namespace ash::personalization_app
