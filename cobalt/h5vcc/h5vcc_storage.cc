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

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "cobalt/h5vcc/h5vcc_storage.h"
#include "cobalt/storage/storage_manager.h"

#include "starboard/common/file.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace h5vcc {

namespace {
const char kTestFileName[] = "cache_test_file.json";

const uint32 kWriteBufferSize = 1024 * 1024;

const uint32 kReadBufferSize = 1024 * 1024;

H5vccStorageWriteTestDictionary WriteTestMessage(std::string error = "",
                                                 uint32 bytes_written = 0) {
  H5vccStorageWriteTestDictionary message;
  message.set_error(error);
  message.set_bytes_written(bytes_written);
  return message;
}

H5vccStorageVerifyTestDictionary VerifyTestMessage(std::string error = "",
                                                   bool verified = false,
                                                   uint32 bytes_read = 0) {
  H5vccStorageVerifyTestDictionary message;
  message.set_error(error);
  message.set_verified(verified);
  message.set_bytes_read(bytes_read);
  return message;
}

}  // namespace

H5vccStorage::H5vccStorage(network::NetworkModule* network_module)
    : network_module_(network_module) {}

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

H5vccStorageWriteTestDictionary H5vccStorage::WriteTest(
    uint32 test_size, std::string test_string) {
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
    return WriteTestMessage(
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
  // kWriteBufferSize per write.
  uint32 total_bytes_written = 0;

  do {
    auto bytes_written = test_file.Write(
        write_buf.data() + total_bytes_written,
        std::min(kWriteBufferSize, test_size - total_bytes_written));
    if (bytes_written <= 0) {
      SbFileDelete(test_file_path.c_str());
      return WriteTestMessage("SbWrite -1 return value error");
    }
    total_bytes_written += bytes_written;
  } while (total_bytes_written < test_size);

  test_file.Flush();

  return WriteTestMessage("", total_bytes_written);
}

H5vccStorageVerifyTestDictionary H5vccStorage::VerifyTest(
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
    return VerifyTestMessage(
        starboard::FormatString("SbFileError: %d while opening ScopedFile: %s",
                                test_file_error, test_file_path.c_str()));
  }

  // Incremental Reads of test_data, copies SbReadAll, using a maximum
  // kReadBufferSize per write.
  uint32 total_bytes_read = 0;

  do {
    char read_buf[kReadBufferSize];
    auto bytes_read = test_file.Read(
        read_buf, std::min(kReadBufferSize, test_size - total_bytes_read));
    if (bytes_read <= 0) {
      SbFileDelete(test_file_path.c_str());
      return VerifyTestMessage("SbRead -1 return value error");
    }

    // Verify read_buf equivalent to a repeated test_string.
    for (auto i = 0; i < bytes_read; ++i) {
      if (read_buf[i] !=
          test_string[(total_bytes_read + i) % test_string.size()]) {
        return VerifyTestMessage(
            "File test data does not match with test data string");
      }
    }

    total_bytes_read += bytes_read;
  } while (total_bytes_read < test_size);

  if (total_bytes_read != test_size) {
    SbFileDelete(test_file_path.c_str());
    return VerifyTestMessage(
        "File test data size does not match kTestDataSize");
  }

  SbFileDelete(test_file_path.c_str());
  return VerifyTestMessage("", true, total_bytes_read);
}

}  // namespace h5vcc
}  // namespace cobalt
