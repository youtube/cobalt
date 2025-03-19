// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_STORAGE_TESTING_H_5_VCC_STORAGE_FOR_TESTING_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_STORAGE_TESTING_H_5_VCC_STORAGE_FOR_TESTING_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_h_5_vcc_storage_verify_test_response.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_h_5_vcc_storage_write_test_response.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_storage/h_5_vcc_storage.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalDOMWindow;

class MODULES_EXPORT H5vccStorageForTesting {
  STATIC_ONLY(H5vccStorageForTesting);

 public:
  // Web-exposed interface:
  static H5vccStorageWriteTestResponse* writeTest(H5vccStorage&,
                                                  uint32_t test_size,
                                                  const String& test_string);
  static H5vccStorageVerifyTestResponse* verifyTest(H5vccStorage&,
                                                    uint32_t test_size,
                                                    const String& test_string);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_STORAGE_TESTING_H_5_VCC_STORAGE_FOR_TESTING_H_
