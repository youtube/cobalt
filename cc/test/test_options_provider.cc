// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_options_provider.h"

#include <limits>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

namespace {

constexpr int kSkottieSerializationHistoryTestPurgePeriod = 5;

}  // namespace

class TestOptionsProvider::DiscardableManager
    : public SkStrikeServer::DiscardableHandleManager,
      public SkStrikeClient::DiscardableHandleManager {
 public:
  DiscardableManager() = default;
  ~DiscardableManager() override = default;

  // SkStrikeServer::DiscardableHandleManager implementation.
  SkDiscardableHandleId createHandle() override { return next_handle_id_++; }
  bool lockHandle(SkDiscardableHandleId handle_id) override {
    CHECK_LT(handle_id, next_handle_id_);
    return true;
  }

  // SkStrikeServer::DiscardableHandleManager::isHandleDeleted implementation.
  bool isHandleDeleted(SkDiscardableHandleId) override { return false; }

  // SkStrikeClient::DiscardableHandleManager implementation.
  bool deleteHandle(SkDiscardableHandleId handle_id) override {
    CHECK_LT(handle_id, next_handle_id_);
    return false;
  }

  // SkStrikeClient::DiscardableHandleManager implementation.
  void notifyCacheMiss(SkStrikeClient::CacheMissType type,
                       int fontSize) override {}

 private:
  SkDiscardableHandleId next_handle_id_ = 1u;
};

TestOptionsProvider::TestOptionsProvider()
    : discardable_manager_(sk_make_sp<DiscardableManager>()),
      strike_server_(discardable_manager_.get()),
      strike_client_(discardable_manager_),
      color_space_(SkColorSpace::MakeSRGB()),
      skottie_serialization_history_(
          kSkottieSerializationHistoryTestPurgePeriod),
      client_paint_cache_(std::numeric_limits<size_t>::max()),
      serialize_options_(this,
                         this,
                         &client_paint_cache_,
                         &strike_server_,
                         color_space_,
                         &skottie_serialization_history_,
                         can_use_lcd_text_,
                         context_supports_distance_field_text_,
                         max_texture_size_),
      deserialize_options_(this,
                           &service_paint_cache_,
                           &strike_client_,
                           &scratch_buffer_,
                           true,
                           nullptr) {}

TestOptionsProvider::~TestOptionsProvider() = default;

sk_sp<SkColorSpace> TestOptionsProvider::color_space() {
  return color_space_;
}

void TestOptionsProvider::PushFonts() {
  std::vector<uint8_t> font_data;
  strike_server_.writeStrikeData(&font_data);
  if (font_data.size() == 0u)
    return;
  CHECK(strike_client_.readStrikeData(font_data.data(), font_data.size()));
}

ImageProvider::ScopedResult TestOptionsProvider::GetRasterContent(
    const DrawImage& draw_image) {
  DCHECK(!draw_image.paint_image().IsPaintWorklet());
  uint32_t image_id = draw_image.paint_image().GetSwSkImage()->uniqueID();
  // Lock and reuse the entry if possible.
  const EntryKey entry_key(TransferCacheEntryType::kImage, image_id);
  if (LockEntryDirect(entry_key)) {
    return ScopedResult(DecodedDrawImage(
        image_id, nullptr, SkSize::MakeEmpty(), draw_image.scale(),
        draw_image.filter_quality(), false, true));
  }

  decoded_images_.push_back(draw_image);
  SkBitmap bitmap;
  const auto& paint_image = draw_image.paint_image();
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(paint_image.width(), paint_image.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Create a transfer cache entry for this image.
  TargetColorParams target_color_params;
  ClientImageTransferCacheEntry cache_entry(
      ClientImageTransferCacheEntry::Image(&bitmap.pixmap()),
      false /* needs_mips */, target_color_params);
  std::vector<uint8_t> data;
  data.resize(cache_entry.SerializedSize());
  if (!cache_entry.Serialize(base::span<uint8_t>(data.data(), data.size()))) {
    return ScopedResult();
  }

  CreateEntryDirect(entry_key, base::span<uint8_t>(data.data(), data.size()));

  return ScopedResult(DecodedDrawImage(
      image_id, nullptr, SkSize::MakeEmpty(), draw_image.scale(),
      draw_image.filter_quality(), false, true));
}

void TestOptionsProvider::ClearPaintCache() {
  client_paint_cache_.FinalizePendingEntries();
  client_paint_cache_.PurgeAll();
  service_paint_cache_.PurgeAll();
}

void TestOptionsProvider::ForcePurgeSkottieSerializationHistory() {
  for (int i = 0; i < kSkottieSerializationHistoryTestPurgePeriod; ++i) {
    skottie_serialization_history_.RequestInactiveAnimationsPurge();
  }
}

}  // namespace cc
