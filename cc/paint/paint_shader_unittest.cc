// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_shader.h"

#include "cc/paint/draw_image.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTileMode.h"

namespace cc {
namespace {

class MockImageGenerator : public FakePaintImageGenerator {
 public:
  explicit MockImageGenerator(const gfx::Size& size)
      : FakePaintImageGenerator(
            SkImageInfo::MakeN32Premul(size.width(), size.height())) {}

  MOCK_METHOD4(GetPixels,
               bool(SkPixmap, size_t, PaintImage::GeneratorClientId, uint32_t));
};

class MockImageProvider : public ImageProvider {
 public:
  MockImageProvider() = default;
  ~MockImageProvider() override = default;

  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override {
    DCHECK(!draw_image.paint_image().IsPaintWorklet());
    draw_image_ = draw_image;

    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    bitmap.eraseColor(SK_ColorBLACK);
    sk_sp<SkImage> image = SkImages::RasterFromBitmap(bitmap);
    return ScopedResult(DecodedDrawImage(image, nullptr, SkSize::MakeEmpty(),
                                         SkSize::Make(1.0f, 1.0f),
                                         draw_image.filter_quality(), true));
  }

  const DrawImage& draw_image() const { return draw_image_; }

 private:
  DrawImage draw_image_;
};

}  // namespace

TEST(PaintShaderTest, RasterizationRectForRecordShaders) {
  SkMatrix local_matrix = SkMatrix::Scale(0.5f, 0.5f);
  auto record_shader = PaintShader::MakePaintRecord(
      PaintRecord(), SkRect::MakeWH(100, 100), SkTileMode::kClamp,
      SkTileMode::kClamp, &local_matrix);

  SkRect tile_rect;
  SkMatrix ctm = SkMatrix::Scale(0.5f, 0.5f);
  EXPECT_TRUE(record_shader->GetRasterizationTileRect(ctm, &tile_rect));
  EXPECT_EQ(tile_rect, SkRect::MakeWH(25, 25));
}

TEST(PaintShaderTest, DecodePaintRecord) {
  // Use a strict mock for the generator. It should never be used when
  // rasterizing this shader, since the decode should be done by the
  // ImageProvider.
  auto generator =
      sk_make_sp<testing::StrictMock<MockImageGenerator>>(gfx::Size(100, 100));
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::GetNextId())
                               .set_paint_image_generator(generator)
                               .TakePaintImage();

  PaintOpBuffer shader_buffer;
  shader_buffer.push<DrawImageOp>(paint_image, 0.f, 0.f);
  SkMatrix local_matrix = SkMatrix::Scale(0.5f, 0.5f);
  auto record_shader = PaintShader::MakePaintRecord(
      shader_buffer.ReleaseAsRecord(), SkRect::MakeWH(100, 100),
      SkTileMode::kClamp, SkTileMode::kClamp, &local_matrix);
  record_shader->set_has_animated_images(true);

  PaintOpBuffer buffer;
  PaintFlags flags;
  flags.setShader(record_shader);
  buffer.push<ScaleOp>(0.5f, 0.5f);
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);

  MockImageProvider image_provider;
  SaveCountingCanvas canvas;
  buffer.Playback(&canvas, PlaybackParams(&image_provider));

  EXPECT_EQ(canvas.draw_rect_, SkRect::MakeWH(100, 100));
  SkShader* shader = canvas.paint_.getShader();
  ASSERT_TRUE(shader);
  SkMatrix decoded_local_matrix;
  SkTileMode xy[2];
  SkImage* skia_image = shader->isAImage(&decoded_local_matrix, xy);
  ASSERT_TRUE(skia_image);
  EXPECT_TRUE(skia_image->isLazyGenerated());
  EXPECT_EQ(xy[0], record_shader->tx());
  EXPECT_EQ(xy[1], record_shader->ty());
  EXPECT_EQ(decoded_local_matrix, SkMatrix::Scale(2.f, 2.f));

  // The rasterization of the shader is internal to skia, so use a raster canvas
  // to verify that the decoded paint does not have the encoded image.
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 100));
  surface->getCanvas()->drawPaint(canvas.paint_);

  // Using the shader requests decode for images at the correct scale.
  EXPECT_TRUE(
      image_provider.draw_image().paint_image().IsSameForTesting(paint_image));
  EXPECT_EQ(image_provider.draw_image().scale().width(), 0.25f);
  EXPECT_EQ(image_provider.draw_image().scale().height(), 0.25f);
}

}  // namespace cc
