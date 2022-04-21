// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_STORAGE_H_
#define COBALT_H5VCC_H5VCC_STORAGE_H_

#include <string>

#include "base/optional.h"
#include "cobalt/h5vcc/h5vcc_storage_verify_test_dictionary.h"
#include "cobalt/h5vcc/h5vcc_storage_write_test_dictionary.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccStorage : public script::Wrappable {
 public:
  explicit H5vccStorage(network::NetworkModule* network_module);
  void ClearCookies();
  void Flush(const base::Optional<bool>& sync);
  bool GetCookiesEnabled();
  void SetCookiesEnabled(bool enabled);

  // Write test_size bytes of a repeating test_string to a test_file in the
  // kSbSystemPathCacheDirectory.
  H5vccStorageWriteTestDictionary WriteTest(uint32 test_size,
                                            std::string test_string);
  // Read the test_file and verify the file data matches the repeating
  // test_string and is at least test_size bytes.
  H5vccStorageVerifyTestDictionary VerifyTest(uint32 test_size,
                                              std::string test_string);

  DEFINE_WRAPPABLE_TYPE(H5vccStorage);

 private:
  network::NetworkModule* network_module_;

  DISALLOW_COPY_AND_ASSIGN(H5vccStorage);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_STORAGE_H_
