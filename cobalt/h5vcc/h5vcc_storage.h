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

#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "cobalt/h5vcc/h5vcc_storage_resource_type_quota_bytes_dictionary.h"
#include "cobalt/h5vcc/h5vcc_storage_set_quota_response.h"
#include "cobalt/h5vcc/h5vcc_storage_verify_test_response.h"
#include "cobalt/h5vcc/h5vcc_storage_write_test_response.h"
#include "cobalt/network/disk_cache/cobalt_backend_impl.h"
#include "cobalt/network/disk_cache/resource_type.h"
#include "cobalt/network/network_module.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/script/wrappable.h"
#include "net/http/http_cache.h"

namespace cobalt {
namespace h5vcc {

class H5vccStorage : public script::Wrappable {
 public:
  explicit H5vccStorage(
      network::NetworkModule* network_module,
      persistent_storage::PersistentSettings* persistent_settings);
  void ClearCookies();
  void Flush(const base::Optional<bool>& sync);
  bool GetCookiesEnabled();
  void SetCookiesEnabled(bool enabled);

  // Write test_size bytes of a repeating test_string to a test_file in the
  // kSbSystemPathCacheDirectory.
  H5vccStorageWriteTestResponse WriteTest(uint32 test_size,
                                          std::string test_string);
  // Read the test_file and verify the file data matches the repeating
  // test_string and is at least test_size bytes.
  H5vccStorageVerifyTestResponse VerifyTest(uint32 test_size,
                                            std::string test_string);

  // Get Quota bytes per disk_cache::ResourceType.
  H5vccStorageResourceTypeQuotaBytesDictionary GetQuota();

  // Set Quota bytes per disk_cache::ResourceType.
  H5vccStorageSetQuotaResponse SetQuota(
      H5vccStorageResourceTypeQuotaBytesDictionary quota);
  void SetAndSaveQuotaForBackend(network::disk_cache::ResourceType type,
                                 uint32_t bytes);

  void EnableCache();

  void DisableCache();

  void ClearCache();

  void ClearCacheOfType(int type_index);

  void ClearServiceWorkerCache();

  DEFINE_WRAPPABLE_TYPE(H5vccStorage);

 private:
  network::NetworkModule* network_module_;

  persistent_storage::PersistentSettings* persistent_settings_;

  net::HttpCache* http_cache_;
  network::disk_cache::CobaltBackendImpl* cache_backend_;

  bool ValidatedCacheBackend();

  DISALLOW_COPY_AND_ASSIGN(H5vccStorage);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_STORAGE_H_
