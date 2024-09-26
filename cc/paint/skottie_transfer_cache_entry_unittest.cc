// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_transfer_cache_entry.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

TEST(SkottieTransferCacheEntryTest, SerializationDeserialization) {
  std::vector<uint8_t> a_data(kLottieDataWithoutAssets1.length());
  a_data.assign(
      reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
      reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
          kLottieDataWithoutAssets1.length());

  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateSerializable(std::move(a_data));

  // Serialize
  auto client_entry(std::make_unique<ClientSkottieTransferCacheEntry>(skottie));
  uint32_t size = client_entry->SerializedSize();
  std::vector<uint8_t> data(size);
  ASSERT_TRUE(client_entry->Serialize(
      base::make_span(static_cast<uint8_t*>(data.data()), size)));

  // De-serialize
  auto entry(std::make_unique<ServiceSkottieTransferCacheEntry>());
  ASSERT_TRUE(entry->Deserialize(
      nullptr, base::make_span(static_cast<uint8_t*>(data.data()), size)));

  EXPECT_EQ(entry->skottie()->id(), skottie->id());
  EXPECT_EQ(entry->skottie()->duration(), skottie->duration());
  EXPECT_EQ(entry->skottie()->size(), skottie->size());
}

}  // namespace cc
