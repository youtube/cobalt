// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/mojom/interest_group/ad_display_size.mojom.h"
#include "url/gurl.h"

namespace blink {

TEST(AdDisplaySizeStructTraitsTest, SerializeAndDeserializeAdSize) {
  blink::AdSize ad_size(300, blink::AdSize::LengthUnit::kPixels, 150,
                        blink::AdSize::LengthUnit::kPixels);

  blink::AdSize ad_size_clone;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<blink::mojom::AdSize>(
      ad_size, ad_size_clone));
  EXPECT_EQ(ad_size, ad_size_clone);
}

TEST(AdDisplaySizeStructTraitsTest, SerializeAndDeserializeAdDescriptor) {
  blink::AdDescriptor ad_descriptor(
      GURL("https://example.test/"),
      blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                    blink::AdSize::LengthUnit::kPixels));

  blink::AdDescriptor ad_descriptor_clone;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<blink::mojom::AdDescriptor>(
      ad_descriptor, ad_descriptor_clone));
  EXPECT_EQ(ad_descriptor, ad_descriptor_clone);
}

}  // namespace blink
