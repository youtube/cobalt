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

#include "cobalt/h5vcc/h5vcc_storage.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/values.h"
#include "cobalt/cache/cache.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/storage/storage_manager.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/cobalt/cobalt_backend_impl.h"
#include "net/disk_cache/cobalt/resource_type.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "starboard/common/file.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace h5vcc {

namespace {

const char kTestFileName[] = "cache_test_file.json";

const uint32 kBufferSize = 16384;  // 16 KB

H5vccStorageWriteTestResponse WriteTestResponse(std::string error = "",
                                                uint32 bytes_written = 0) {
  H5vccStorageWriteTestResponse response;
  response.set_error(error);
  response.set_bytes_written(bytes_written);
  return response;
}

H5vccStorageVerifyTestResponse VerifyTestResponse(std::string error = "",
                                                  bool verified = false,
                                                  uint32 bytes_read = 0) {
  H5vccStorageVerifyTestResponse response;
  response.set_error(error);
  response.set_verified(verified);
  response.set_bytes_read(bytes_read);
  return response;
}

H5vccStorageSetQuotaResponse SetQuotaResponse(std::string error = "",
                                              bool success = false) {
  H5vccStorageSetQuotaResponse response;
  response.set_success(success);
  response.set_error(error);
  return response;
}

void ClearCacheHelper(disk_cache::Backend* backend) {
  backend->DoomAllEntries(base::DoNothing());
}

void ClearCacheOfTypeHelper(disk_cache::ResourceType type,
                            disk_cache::CobaltBackendImpl* backend) {
  backend->DoomAllEntriesOfType(type, base::DoNothing());
}

}  // namespace

H5vccStorage::H5vccStorage(
    network::NetworkModule* network_module,
    persistent_storage::PersistentSettings* persistent_settings)
    : network_module_(network_module),
      persistent_settings_(persistent_settings) {
  http_cache_ = nullptr;
  cache_backend_ = nullptr;
  if (network_module == nullptr) {
    return;
  }
  auto url_request_context = network_module_->url_request_context();
  if (url_request_context->using_http_cache()) {
    http_cache_ = url_request_context->http_transaction_factory()->GetCache();
  }
}

void H5vccStorage::ClearCookies() {
  net::CookieStore* cookie_store =
      network_module_->url_request_context()->cookie_store();
  auto* cookie_monster = static_cast<net::CookieMonster*>(cookie_store);
  network_module_->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&net::CookieMonster::DeleteAllMatchingInfoAsync,
                 base::Unretained(cookie_monster), net::CookieDeletionInfo(),
                 base::Passed(net::CookieStore::DeleteCallback())));
}

void H5vccStorage::Flush(const base::Optional<bool>& sync) {
  if (sync.value_or(false) == true) {
    DLOG(WARNING) << "Synchronous flush is not supported.";
  }

  network_module_->storage_manager()->FlushNow(base::Closure());
}

bool H5vccStorage::GetCookiesEnabled() {
  return network_module_->network_delegate()->cookies_enabled();
}

void H5vccStorage::SetCookiesEnabled(bool enabled) {
  network_module_->network_delegate()->set_cookies_enabled(enabled);
}

H5vccStorageWriteTestResponse H5vccStorage::WriteTest(uint32 test_size,
                                                      std::string test_string) {
  // Get cache_dir path.
  std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
  SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                  kSbFileMaxPath);

  // Delete the contents of cache_dir.
  starboard::SbFileDeleteRecursive(cache_dir.data(), true);

  // Try to Create the test_file.
  std::string test_file_path =
      std::string(cache_dir.data()) + kSbFileSepString + kTestFileName;
  SbFileError test_file_error;
  starboard::ScopedFile test_file(test_file_path.c_str(),
                                  kSbFileOpenAlways | kSbFileWrite, NULL,
                                  &test_file_error);

  if (test_file_error != kSbFileOk) {
    return WriteTestResponse(
        starboard::FormatString("SbFileError: %d while opening ScopedFile: %s",
                                test_file_error, test_file_path.c_str()));
  }

  // Repeatedly write test_string to test_size bytes of write_buffer.
  std::string write_buf;
  int iterations = test_size / test_string.length();
  for (int i = 0; i < iterations; ++i) {
    write_buf.append(test_string);
  }
  write_buf.append(test_string.substr(0, test_size % test_string.length()));

  // Incremental Writes of test_data, copies SbWriteAll, using a maximum
  // kBufferSize per write.
  uint32 total_bytes_written = 0;

  do {
    auto bytes_written =
        test_file.Write(write_buf.data() + total_bytes_written,
                        std::min(kBufferSize, test_size - total_bytes_written));
    if (bytes_written <= 0) {
      SbFileDelete(test_file_path.c_str());
      return WriteTestResponse("SbWrite -1 return value error");
    }
    total_bytes_written += bytes_written;
  } while (total_bytes_written < test_size);

  test_file.Flush();

  return WriteTestResponse("", total_bytes_written);
}

H5vccStorageVerifyTestResponse H5vccStorage::VerifyTest(
    uint32 test_size, std::string test_string) {
  std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
  SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                  kSbFileMaxPath);

  std::string test_file_path =
      std::string(cache_dir.data()) + kSbFileSepString + kTestFileName;
  SbFileError test_file_error;
  starboard::ScopedFile test_file(test_file_path.c_str(),
                                  kSbFileOpenOnly | kSbFileRead, NULL,
                                  &test_file_error);

  if (test_file_error != kSbFileOk) {
    return VerifyTestResponse(
        starboard::FormatString("SbFileError: %d while opening ScopedFile: %s",
                                test_file_error, test_file_path.c_str()));
  }

  // Incremental Reads of test_data, copies SbReadAll, using a maximum
  // kBufferSize per write.
  uint32 total_bytes_read = 0;

  do {
    auto read_buffer = std::make_unique<char[]>(kBufferSize);
    auto bytes_read = test_file.Read(
        read_buffer.get(), std::min(kBufferSize, test_size - total_bytes_read));
    if (bytes_read <= 0) {
      SbFileDelete(test_file_path.c_str());
      return VerifyTestResponse("SbRead -1 return value error");
    }

    // Verify read_buffer equivalent to a repeated test_string.
    for (auto i = 0; i < bytes_read; ++i) {
      if (read_buffer.get()[i] !=
          test_string[(total_bytes_read + i) % test_string.size()]) {
        return VerifyTestResponse(
            "File test data does not match with test data string");
      }
    }

    total_bytes_read += bytes_read;
  } while (total_bytes_read < test_size);

  if (total_bytes_read != test_size) {
    SbFileDelete(test_file_path.c_str());
    return VerifyTestResponse(
        "File test data size does not match kTestDataSize");
  }

  SbFileDelete(test_file_path.c_str());
  return VerifyTestResponse("", true, total_bytes_read);
}

H5vccStorageSetQuotaResponse H5vccStorage::SetQuota(
    H5vccStorageResourceTypeQuotaBytesDictionary quota) {
  if (!quota.has_other() || !quota.has_html() || !quota.has_css() ||
      !quota.has_image() || !quota.has_font() || !quota.has_splash() ||
      !quota.has_uncompiled_js() || !quota.has_compiled_js() ||
      !quota.has_cache_api() || !quota.has_service_worker_js()) {
    return SetQuotaResponse(
        "H5vccStorageResourceTypeQuotaBytesDictionary input parameter missing "
        "required fields.");
  }

  if (quota.other() < 0 || quota.html() < 0 || quota.css() < 0 ||
      quota.image() < 0 || quota.font() < 0 || quota.splash() < 0 ||
      quota.uncompiled_js() < 0 || quota.compiled_js() < 0 ||
      quota.cache_api() < 0 || quota.service_worker_js() < 0) {
    return SetQuotaResponse(
        "H5vccStorageResourceTypeQuotaBytesDictionary input parameter fields "
        "cannot have a negative value.");
  }

  auto quota_total = quota.other() + quota.html() + quota.css() +
                     quota.image() + quota.font() + quota.splash() +
                     quota.uncompiled_js() + quota.compiled_js() +
                     quota.cache_api() + quota.service_worker_js();

  uint32_t max_quota_size = 24 * 1024 * 1024;
#if SB_API_VERSION >= 14
  max_quota_size = kSbMaxSystemPathCacheDirectorySize;
#endif
  // Assume the non-http-cache memory in kSbSystemPathCacheDirectory
  // is less than 1 mb and subtract this from the max_quota_size.
  max_quota_size -= (1 << 20);

  if (quota_total != max_quota_size) {
    return SetQuotaResponse(starboard::FormatString(
        "H5vccStorageResourceTypeQuotaDictionary input parameter field values "
        "sum (%d) is not equal to the max cache size (%d).",
        quota_total, max_quota_size));
  }

  ValidatedCacheBackend();

  // Write to persistent storage with the new quota values.
  SetAndSaveQuotaForBackend(disk_cache::kOther,
                            static_cast<uint32_t>(quota.other()));
  SetAndSaveQuotaForBackend(disk_cache::kHTML,
                            static_cast<uint32_t>(quota.html()));
  SetAndSaveQuotaForBackend(disk_cache::kCSS,
                            static_cast<uint32_t>(quota.css()));
  SetAndSaveQuotaForBackend(disk_cache::kImage,
                            static_cast<uint32_t>(quota.image()));
  SetAndSaveQuotaForBackend(disk_cache::kFont,
                            static_cast<uint32_t>(quota.font()));
  SetAndSaveQuotaForBackend(disk_cache::kSplashScreen,
                            static_cast<uint32_t>(quota.splash()));
  SetAndSaveQuotaForBackend(disk_cache::kUncompiledScript,
                            static_cast<uint32_t>(quota.uncompiled_js()));
  SetAndSaveQuotaForBackend(disk_cache::kCompiledScript,
                            static_cast<uint32_t>(quota.compiled_js()));
  SetAndSaveQuotaForBackend(disk_cache::kCacheApi,
                            static_cast<uint32_t>(quota.cache_api()));
  SetAndSaveQuotaForBackend(disk_cache::kServiceWorkerScript,
                            static_cast<uint32_t>(quota.service_worker_js()));
  return SetQuotaResponse("", true);
}

void H5vccStorage::SetAndSaveQuotaForBackend(disk_cache::ResourceType type,
                                             uint32_t bytes) {
  if (cache_backend_) {
    cache_backend_->UpdateSizes(type, bytes);

    if (bytes == 0) {
      network_module_->task_runner()->PostTask(
          FROM_HERE, base::Bind(&ClearCacheOfTypeHelper, type,
                                base::Unretained(cache_backend_)));
    }
  }
  cobalt::cache::Cache::GetInstance()->Resize(type, bytes);
}

H5vccStorageResourceTypeQuotaBytesDictionary H5vccStorage::GetQuota() {
  // Return persistent storage quota values.
  H5vccStorageResourceTypeQuotaBytesDictionary quota;
  if (!ValidatedCacheBackend()) {
    return quota;
  }

  quota.set_other(cache_backend_->GetQuota(disk_cache::kOther));
  quota.set_html(cache_backend_->GetQuota(disk_cache::kHTML));
  quota.set_css(cache_backend_->GetQuota(disk_cache::kCSS));
  quota.set_image(cache_backend_->GetQuota(disk_cache::kImage));
  quota.set_font(cache_backend_->GetQuota(disk_cache::kFont));
  quota.set_splash(cache_backend_->GetQuota(disk_cache::kSplashScreen));
  quota.set_uncompiled_js(
      cache_backend_->GetQuota(disk_cache::kUncompiledScript));
  quota.set_compiled_js(
      cobalt::cache::Cache::GetInstance()
          ->GetMaxCacheStorageInBytes(disk_cache::kCompiledScript)
          .value());
  quota.set_cache_api(cache_backend_->GetQuota(disk_cache::kCacheApi));
  quota.set_service_worker_js(
      cache_backend_->GetQuota(disk_cache::kServiceWorkerScript));

  uint32_t max_quota_size = 24 * 1024 * 1024;
#if SB_API_VERSION >= 14
  max_quota_size = kSbMaxSystemPathCacheDirectorySize;
#endif
  // Assume the non-http-cache memory in kSbSystemPathCacheDirectory
  // is less than 1 mb and subtract this from the max_quota_size.
  max_quota_size -= (1 << 20);

  quota.set_total(max_quota_size);

  return quota;
}

void H5vccStorage::EnableCache() {
  persistent_settings_->SetPersistentSetting(
      disk_cache::kCacheEnabledPersistentSettingsKey,
      std::make_unique<base::Value>(true));

  cobalt::cache::Cache::GetInstance()->set_enabled(true);

  if (http_cache_) {
    http_cache_->set_mode(net::HttpCache::Mode::NORMAL);
  }
}

void H5vccStorage::DisableCache() {
  persistent_settings_->SetPersistentSetting(
      disk_cache::kCacheEnabledPersistentSettingsKey,
      std::make_unique<base::Value>(false));

  cobalt::cache::Cache::GetInstance()->set_enabled(false);

  if (http_cache_) {
    http_cache_->set_mode(net::HttpCache::Mode::DISABLE);
  }
}

void H5vccStorage::ClearCache() {
  if (ValidatedCacheBackend()) {
    network_module_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ClearCacheHelper, base::Unretained(cache_backend_)));
  }
  cobalt::cache::Cache::GetInstance()->DeleteAll();
}

bool H5vccStorage::ValidatedCacheBackend() {
  if (!http_cache_) {
    return false;
  }
  if (cache_backend_) {
    return true;
  }
  cache_backend_ = static_cast<disk_cache::CobaltBackendImpl*>(
      http_cache_->GetCurrentBackend());
  return cache_backend_ != nullptr;
}

}  // namespace h5vcc
}  // namespace cobalt
