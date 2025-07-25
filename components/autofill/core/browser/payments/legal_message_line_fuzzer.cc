// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Prepare fuzzed parameters.
  absl::optional<base::Value> legal_message =
      base::JSONReader::Read(provider.ConsumeRandomLengthString());
  if (!legal_message || !legal_message->is_dict())
    return 0;
  bool escape_apostrophes = provider.ConsumeBool();

  // Run tested code.
  autofill::LegalMessageLines lines;
  autofill::LegalMessageLine::Parse(legal_message->GetDict(), &lines,
                                    escape_apostrophes);

  return 0;
}
