// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/test/base/testing_profile.h"
#endif

namespace apps {

void EnsureRepresentationsLoaded(gfx::ImageSkia& output_image_skia) {
  for (auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    // Force the icon to be loaded.
    output_image_skia.GetRepresentation(
        ui::GetScaleForResourceScaleFactor(scale_factor));
  }
}

void LoadDefaultIcon(gfx::ImageSkia& output_image_skia, int resource_id) {
  base::RunLoop run_loop;
  apps::LoadIconFromResource(
      apps::IconType::kUncompressed, kSizeInDip, resource_id,
      false /* is_placeholder_icon */, apps::IconEffects::kNone,
      base::BindOnce(
          [](gfx::ImageSkia* image, base::OnceClosure load_app_icon_callback,
             apps::IconValuePtr icon) {
            *image = icon->uncompressed;
            std::move(load_app_icon_callback).Run();
          },
          &output_image_skia, run_loop.QuitClosure()));
  run_loop.Run();
  EnsureRepresentationsLoaded(output_image_skia);
}

void VerifyIcon(const gfx::ImageSkia& src, const gfx::ImageSkia& dst) {
  ASSERT_FALSE(src.isNull());
  ASSERT_FALSE(dst.isNull());

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (auto& scale_factor : scale_factors) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    ASSERT_TRUE(src.HasRepresentation(scale));
    ASSERT_TRUE(dst.HasRepresentation(scale));
    ASSERT_TRUE(
        gfx::test::AreBitmapsEqual(src.GetRepresentation(scale).GetBitmap(),
                                   dst.GetRepresentation(scale).GetBitmap()));
  }
}

void VerifyCompressedIcon(const std::vector<uint8_t>& src_data,
                          const apps::IconValue& icon) {
  ASSERT_EQ(apps::IconType::kCompressed, icon.icon_type);
  ASSERT_FALSE(icon.is_placeholder_icon);
  ASSERT_FALSE(icon.compressed.empty());
  ASSERT_EQ(src_data, icon.compressed);
}

SkBitmap CreateSquareIconBitmap(int size_px, SkColor solid_color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size_px, size_px);
  bitmap.eraseColor(solid_color);
  return bitmap;
}

gfx::ImageSkia CreateSquareIconImageSkia(int size_dp, SkColor solid_color) {
  gfx::ImageSkia image;
  for (auto& scale_factor : ui::GetSupportedResourceScaleFactors()) {
    int icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(size_dp, size_dp), scale_factor)
            .width();
    SkBitmap bitmap = CreateSquareIconBitmap(icon_size_in_px, solid_color);
    image.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale_factor));
  }
  return image;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
FakeIconLoader::FakeIconLoader(apps::AppServiceProxy* proxy) : proxy_(proxy) {}

std::unique_ptr<apps::IconLoader::Releaser> FakeIconLoader::LoadIconFromIconKey(
    apps::AppType app_type,
    const std::string& app_id,
    const apps::IconKey& icon_key,
    apps::IconType icon_type,
    int32_t size_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  if (proxy_) {
    proxy_->ReadIconsForTesting(app_type, app_id, size_in_dip, icon_key,
                                icon_type, std::move(callback));
  }
  return nullptr;
}

FakePublisherForIconTest::FakePublisherForIconTest(apps::AppServiceProxy* proxy,
                                                   apps::AppType app_type)
    : AppPublisher(proxy) {
  RegisterPublisher(app_type);
}

void FakePublisherForIconTest::GetCompressedIconData(
    const std::string& app_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  apps::GetWebAppCompressedIconData(proxy()->profile(), app_id, size_in_dip,
                                    scale_factor, std::move(callback));
}
#endif

}  // namespace apps
