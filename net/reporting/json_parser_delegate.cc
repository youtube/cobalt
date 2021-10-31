// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/json_parser_delegate.h"

#include "base/json/json_reader.h"
#include "base/values.h"

namespace net {

namespace {
constexpr int kMaxJsonSize = 16 * 1024;
constexpr int kMaxJsonDepth = 5;
}  // namespace

void InProcessJSONParser::ParseJson(const std::string& unsafe_json,
                                    SuccessCallback success_callback,
                                    FailureCallback failure_callback) const {
  if (unsafe_json.size() > kMaxJsonSize) {
    std::move(failure_callback).Run();
    return;
  }

  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(unsafe_json, base::JSON_PARSE_RFC, kMaxJsonDepth);
  if (value)
    std::move(success_callback).Run(std::move(value));
  else
    std::move(failure_callback).Run();
}

}  // namespace net
