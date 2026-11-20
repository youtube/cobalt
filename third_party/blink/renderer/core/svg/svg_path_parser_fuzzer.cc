// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_parser.h"

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/blink/renderer/core/svg/svg_path_string_source.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;
  blink::String input_string = blink::String::FromUtf8WithLatin1Fallback(data);
  blink::SVGPathStringSource source(input_string);
  class NullConsumer {
   public:
    void EmitSegment(const blink::PathSegmentData&) {}
  } null_consumer;
  blink::svg_path_parser::ParsePath(source, null_consumer);
  return 0;
}
