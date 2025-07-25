// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"

namespace blink {

int FuzzTokenizer(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  FuzzedDataProvider fuzzed_data_provider(data, size);

  // Use the first byte of fuzz data to randomize the tokenizer options.
  HTMLParserOptions options;
  options.scripting_flag = fuzzed_data_provider.ConsumeBool();

  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input;
  while (fuzzed_data_provider.RemainingBytes() > 0) {
    // The tokenizer deals with incremental strings as they are received.
    // Split the input into a bunch of small chunks to throw partial tokens
    // at the tokenizer and exercise the state machine and resumption.
    String chunk = fuzzed_data_provider.ConsumeRandomLengthString(32);
    input.Append(SegmentedString(chunk));
    // HTMLTokenizer::NextToken() returns the token on success. Clear() must
    // be called after every successful token.
    while (HTMLToken* token = tokenizer->NextToken(input)) {
      token->Clear();
    }
  }
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Need at least 2 bytes for the options flags and one byte of test data.
  // Avoid huge inputs which can cause non-actionable timeout crashes.
  if (size >= 3 && size <= 16384)
    blink::FuzzTokenizer(data, size);

  return 0;
}
