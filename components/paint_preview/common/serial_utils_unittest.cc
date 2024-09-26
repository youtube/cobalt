// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/serial_utils.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

TEST(PaintPreviewSerialUtils, TestMakeEmptyPicture) {
  sk_sp<SkPicture> pic = MakeEmptyPicture();
  ASSERT_NE(pic, nullptr);
  auto data = pic->serialize();
  ASSERT_NE(data, nullptr);
  EXPECT_GE(data->size(), 0U);
}

TEST(PaintPreviewSerialUtils, TestTransformedPictureProcs) {
  auto pic = MakeEmptyPicture();
  uint32_t content_id = pic->uniqueID();
  const base::UnguessableToken kFrameGuid = base::UnguessableToken::Create();
  PictureSerializationContext picture_ctx;
  EXPECT_TRUE(picture_ctx.content_id_to_embedding_token
                  .insert(std::make_pair(content_id, kFrameGuid))
                  .second);
  auto new_clip = SkRect::MakeXYWH(10, 20, 30, 40);
  EXPECT_TRUE(picture_ctx.content_id_to_transformed_clip
                  .insert(std::make_pair(content_id, new_clip))
                  .second);

  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, nullptr);

  DeserializationContext deserial_ctx;
  SkDeserialProcs deserial_procs = MakeDeserialProcs(&deserial_ctx);
  EXPECT_EQ(deserial_procs.fPictureCtx, &deserial_ctx);

  // Check that serializing then deserialize the picture works produces a
  // correct clip rect.
  sk_sp<SkData> serial_pic_data =
      serial_procs.fPictureProc(pic.get(), serial_procs.fPictureCtx);
  sk_sp<SkPicture> deserial_pic = deserial_procs.fPictureProc(
      serial_pic_data->data(), serial_pic_data->size(),
      deserial_procs.fPictureCtx);
  EXPECT_TRUE(deserial_ctx.count(content_id));
  EXPECT_EQ(deserial_ctx[content_id].x(), new_clip.x());
  EXPECT_EQ(deserial_ctx[content_id].y(), new_clip.y());
  EXPECT_EQ(deserial_ctx[content_id].width(), new_clip.width());
  EXPECT_EQ(deserial_ctx[content_id].height(), new_clip.height());
}

TEST(PaintPreviewSerialUtils, TestSerialPictureNotInMap) {
  PictureSerializationContext picture_ctx;

  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, nullptr);

  auto pic = MakeEmptyPicture();
  EXPECT_EQ(serial_procs.fPictureProc(pic.get(), serial_procs.fPictureCtx),
            nullptr);
}

// Skip this on Android as we only have system fonts in this test and Android
// doesn't serialize those.
#if !BUILDFLAG(IS_ANDROID)
TEST(PaintPreviewSerialUtils, TestSerialTypeface) {
  PictureSerializationContext picture_ctx;

  auto typeface = SkTypeface::MakeDefault();
  TypefaceUsageMap usage_map;
  std::unique_ptr<GlyphUsage> usage =
      std::make_unique<SparseGlyphUsage>(typeface->countGlyphs());
  usage->Set(0);
  usage->Set('a');
  usage->Set('b');
  EXPECT_TRUE(
      usage_map.insert(std::make_pair(typeface->uniqueID(), std::move(usage)))
          .second);
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, nullptr);

  auto final_data =
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx);
  ASSERT_TRUE(final_data);
  EXPECT_GT(typeface_ctx.finished.count(typeface->uniqueID()), 0U);
  auto original_data = typeface->serialize();
  ASSERT_NE(original_data->size(), final_data->size());
}
#endif

#if BUILDFLAG(IS_ANDROID)
TEST(PaintPreviewSerialUtils, TestSerialAndroidSystemTypeface) {
  PictureSerializationContext picture_ctx;

  // This is a system font serialization of the data will be skipped.
  auto typeface = SkTypeface::MakeFromName("sans-serif", SkFontStyle::Bold());
  TypefaceUsageMap usage_map;
  std::unique_ptr<GlyphUsage> usage =
      std::make_unique<SparseGlyphUsage>(typeface->countGlyphs());
  usage->Set(0);
  usage->Set('a');
  usage->Set('b');
  EXPECT_TRUE(
      usage_map.insert(std::make_pair(typeface->uniqueID(), std::move(usage)))
          .second);
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, nullptr);

  auto final_data =
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx);
  ASSERT_TRUE(final_data);
  EXPECT_GT(typeface_ctx.finished.count(typeface->uniqueID()), 0U);
  auto original_data = typeface->serialize();
  ASSERT_EQ(original_data->size(), final_data->size());
  ASSERT_EQ(
      0, memcmp(original_data->data(), final_data->data(), final_data->size()));
}
#endif

TEST(PaintPreviewSerialUtils, TestSerialNoTypefaceInMap) {
  PictureSerializationContext picture_ctx;

  auto typeface = SkTypeface::MakeDefault();
  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, nullptr);

  EXPECT_NE(
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx),
      nullptr);
  EXPECT_GT(typeface_ctx.finished.count(typeface->uniqueID()), 0U);
  // Still serialize font metadata for lookup if the font is serialized again.
  EXPECT_NE(
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx),
      nullptr);
}

TEST(PaintPreviewSerialUtils, TestImageContextLimitBudget) {
  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(1, 19);
  SkCanvas canvas1(bitmap1);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorRED);
  canvas1.drawRect(SkRect::MakeWH(1, 4), paint);
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(SkRect::MakeWH(40, 40));
  canvas->drawImage(bitmap1.asImage(), 0, 0);
  canvas->drawImage(bitmap1.asImage(), 0, 0);
  canvas->drawImage(bitmap1.asImage(), 0, 0);
  auto pic = recorder.finishRecordingAsPicture();

  PictureSerializationContext picture_ctx;
  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;
  ictx.remaining_image_size = 200;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, &ictx);
  EXPECT_EQ(serial_procs.fImageCtx, &ictx);

  sk_sp<SkData> data = pic->serialize(&serial_procs);
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(ictx.memory_budget_exceeded);
  SkDeserialProcs deserial_procs;
  size_t deserialized_images = 0;
  deserial_procs.fImageCtx = &deserialized_images;
  deserial_procs.fImageProc = [](const void* data, size_t length,
                                 void* ctx) -> sk_sp<SkImage> {
    if (length > 0U) {
      size_t* images = reinterpret_cast<size_t*>(ctx);
      *images += 1;
    }
    return nullptr;
  };
  SkPicture::MakeFromData(data.get(), &deserial_procs);
  EXPECT_EQ(deserialized_images, 2U);
}

TEST(PaintPreviewSerialUtils, TestImageContextLimitSize) {
  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(1, 19);
  SkCanvas canvas1(bitmap1);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorRED);
  canvas1.drawRect(SkRect::MakeWH(1, 4), paint);
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(20, 20);
  SkCanvas canvas2(bitmap2);
  canvas2.drawRect(SkRect::MakeWH(20, 5), paint);
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(SkRect::MakeWH(40, 40));
  canvas->drawImage(bitmap1.asImage(), 0, 0);
  canvas->drawImage(bitmap2.asImage(), 0, 0);
  auto pic = recorder.finishRecordingAsPicture();

  PictureSerializationContext picture_ctx;
  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);
  ImageSerializationContext ictx;
  ictx.max_decoded_image_size_bytes = 200;

  SkSerialProcs serial_procs =
      MakeSerialProcs(&picture_ctx, &typeface_ctx, &ictx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fImageCtx, &ictx);
  EXPECT_EQ(serial_procs.fImageCtx, &ictx);

  sk_sp<SkData> data = pic->serialize(&serial_procs);
  EXPECT_NE(data, nullptr);
  EXPECT_FALSE(ictx.memory_budget_exceeded);
  SkDeserialProcs deserial_procs;
  size_t deserialized_images = 0;
  deserial_procs.fImageCtx = &deserialized_images;
  deserial_procs.fImageProc = [](const void* data, size_t length,
                                 void* ctx) -> sk_sp<SkImage> {
    if (length > 0U) {
      size_t* images = reinterpret_cast<size_t*>(ctx);
      *images += 1;
    }
    return nullptr;
  };
  SkPicture::MakeFromData(data.get(), &deserial_procs);
  EXPECT_EQ(deserialized_images, 1U);
}

}  // namespace paint_preview
