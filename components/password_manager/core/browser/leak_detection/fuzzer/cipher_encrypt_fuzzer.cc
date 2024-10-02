// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string payload(reinterpret_cast<const char*>(data), size);
  std::string key;
  absl::optional<std::string> cipher =
      password_manager::CipherEncrypt(payload, &key);
  return 0;
}

}  // namespace password_manager
