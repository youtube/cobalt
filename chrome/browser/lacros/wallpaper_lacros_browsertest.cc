// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace {
const char kFakeExtensionId[] = "abcdefghijklmnopqrstuvxwyzabcdef";
const char kFakeExtensionName[] = "Fake Extension Name";
const char kTestWallpaperFilename[] = "some-wallpaper-name";
}  // namespace

class WallpaperLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  WallpaperLacrosBrowserTest() = default;

  WallpaperLacrosBrowserTest(const WallpaperLacrosBrowserTest&) = delete;
  WallpaperLacrosBrowserTest& operator=(const WallpaperLacrosBrowserTest&) =
      delete;

  ~WallpaperLacrosBrowserTest() override = default;

  static std::vector<uint8_t> CreateJpeg();
};

std::vector<uint8_t> WallpaperLacrosBrowserTest::CreateJpeg() {
  const int kWidth = 100;
  const int kHeight = 100;
  const SkColor kRed = SkColorSetRGB(255, 0, 0);
  const int kQuality = 80;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kWidth, kHeight);
  bitmap.eraseColor(kRed);

  std::vector<uint8_t> jpg_data;

  bool encoded = gfx::JPEGCodec::Encode(bitmap, kQuality, &jpg_data);
  if (!encoded) {
    LOG(ERROR) << "Failed to encode a sample JPEG wallpaper image";
    jpg_data.clear();
  }

  return jpg_data;
}

// Tests that setting the wallpaper via crosapi works
IN_PROC_BROWSER_TEST_F(WallpaperLacrosBrowserTest, SetWallpaper) {
  auto* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::Wallpaper>()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();

  settings->data = CreateJpeg();
  ASSERT_FALSE(settings->data.empty());

  settings->layout = crosapi::mojom::WallpaperLayout::kCenter;
  settings->filename = kTestWallpaperFilename;

  base::test::TestFuture<crosapi::mojom::SetWallpaperResultPtr> future;
  lacros_service->GetRemote<crosapi::mojom::Wallpaper>()->SetWallpaper(
      std::move(settings), kFakeExtensionId, kFakeExtensionName,
      future.GetCallback());
  auto result = future.Take();

  // If a valid thumbnail is returned it means Ash set the wallpaper.
  ASSERT_FALSE(result->is_error_message());
  ASSERT_TRUE(result->is_thumbnail_data());
  EXPECT_FALSE(result->get_thumbnail_data().empty());
}
