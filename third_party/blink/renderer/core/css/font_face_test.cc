// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

class FontFaceTest : public PageTestBase {};

using ::testing::NotNull;

// Simple regression test for nullptr dereference found in
// `FontFace::Create()` via `-fsanitize=nullptr`. This is part of
// extending UBSan hardening in Chromium (see
// https://crbug.com/524864598).
//
// This test passes if it does not crash.
TEST_F(FontFaceTest, Create) {
  auto family = AtomicString::FromUtf8("Family");

  auto* twenty_pixels =
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>("20px");
  EXPECT_THAT(
      FontFace::Create(GetFrame().DomWindow()->GetExecutionContext(), family,
                       twenty_pixels, FontFaceDescriptors::Create()),
      NotNull());

  auto* random_source =
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          "random()");
  EXPECT_THAT(
      FontFace::Create(GetFrame().DomWindow()->GetExecutionContext(), family,
                       random_source, FontFaceDescriptors::Create()),
      NotNull());
}

}  // namespace
}  // namespace blink
