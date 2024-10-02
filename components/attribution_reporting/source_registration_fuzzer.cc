// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <iostream>
#include <string>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    base::i18n::InitializeICU();
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

}  // namespace

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  static Environment env;

  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  absl::optional<base::Value> input = base::JSONReader::Read(
      native_input, base::JSONParserOptions::JSON_PARSE_RFC);
  if (!input || !input->is_dict())
    return;

  std::ignore = SourceRegistration::Parse(std::move(*input).TakeDict());
}

}  // namespace attribution_reporting
