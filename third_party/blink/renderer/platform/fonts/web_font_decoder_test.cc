// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"

#include <optional>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

// Regression test for a font that triggers a large number of OTS warnings
// during decoding. Without a bound on the accumulated error string, processing
// these warnings dominated decode time and made the OpenType math fuzzer time
// out. Verify that the reported error string stays within the budget.
TEST(WebFontDecoderTest, ErrorStringIsBoundedOnPathologicalFont) {
  std::optional<Vector<char>> font_data = test::ReadFromFile(
      test::PlatformTestDataPath("open_type_math_support_fuzzer_timeout.ttf"));
  ASSERT_TRUE(font_data);

  auto font_buffer = SharedBuffer::Create(std::move(*font_data));
  base::expected<DecodedWebFont, String> decoded_result =
      DecodedWebFont::Create(font_buffer.get());

  // Messages are accepted until the accumulated string reaches the ~4096 byte
  // budget, so the result may overshoot by at most one final message. The
  // point of the bound is that the string stays small instead of growing
  // without limit (which is what caused the fuzzer timeout), so allow generous
  // slack over the budget for that trailing message.
  ASSERT_FALSE(decoded_result.has_value());
  EXPECT_LT(decoded_result.error().length(), 8192u);
}

TEST(WebFontDecoderTest, EmptyBufferIsError) {
  auto font_buffer = SharedBuffer::Create();
  base::expected<DecodedWebFont, String> decoded_result =
      DecodedWebFont::Create(font_buffer.get());
  ASSERT_FALSE(decoded_result.has_value());
}

}  // namespace blink
