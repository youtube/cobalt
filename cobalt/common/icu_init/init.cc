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

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unicode/utypes.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

namespace cobalt {
namespace common {
namespace icu_init {

namespace {

constexpr std::string_view kIcuDataFileName = "icudtl.dat";

pthread_once_t g_initialization_once = PTHREAD_ONCE_INIT;
bool g_initialization_result = false;

// Gets the full path to the ICU data file.
std::string GetIcuDataPath() {
  std::vector<char> base_path(kSbFileMaxPath);
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, base_path.data(),
                                base_path.size());
  SB_CHECK(result) << "Failed to get ICU data path.";
  std::string data_path(base_path.data());
  data_path += kSbFileSepString;
  data_path += kIcuDataFileName;
  return data_path;
}

off_t GetIcuDataLength(const std::string& data_path) {
  struct stat st;
  int stat_result = stat(data_path.c_str(), &st);
  if (stat_result != 0 || st.st_size <= 0) {
    SB_CHECK(stat_result != -1) << "Failed to stat ICU data " << data_path
                                << ", error " << strerror(errno);
    SB_CHECK(stat_result == 0 && st.st_size > 0)
        << "ICU data file unexpectedly has zero length.";
    return -1;
  }
  return st.st_size;
}

// Memory-maps the ICU data file and returns a pointer to the mapped region.
void* MmapIcuData(const std::string& data_path, off_t length) {
  int fd = open(data_path.c_str(), O_RDONLY);
  if (fd == -1) {
    SB_LOG(ERROR) << "Failed to open ICU data file " << data_path << ", error "
                  << strerror(errno);
    return nullptr;
  }

  void* icu_data = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);

  if (icu_data == MAP_FAILED) {
    SB_LOG(ERROR) << "Failed to mmap ICU data " << data_path << ", error "
                  << strerror(errno);
    return nullptr;
  }
  return icu_data;
}

bool SetIcuDataPointer(void* icu_data) {
  UErrorCode error_code = U_ZERO_ERROR;
  udata_setCommonData(icu_data, &error_code);
  if (U_FAILURE(error_code)) {
    SB_CHECK(U_SUCCESS(error_code))
        << "Failed to set ICU common data pointer, error "
        << u_errorName(error_code);
    return false;
  }
  return true;
}

void PrintIcuNotLoadedWarning() {
  SB_LOG(WARNING) << "ICU Database could not be loaded!";
}

// Initialize the ICU using the file named kIcuDataFileName in the
// kSbSystemPathContentDirectory folder. Note that this gives the ICU its
// database, but it does not actually set a default timezone or locale.
void InitializeIcuDatabase() {
  std::string data_path = GetIcuDataPath();

  off_t length = GetIcuDataLength(data_path);
  if (length <= 0) {
    PrintIcuNotLoadedWarning();
    return;
  }

  void* icu_data = MmapIcuData(data_path, length);
  if (!icu_data) {
    PrintIcuNotLoadedWarning();
    return;
  }

  // Inform the OS that the mapped data is accessed randomly.
  madvise(icu_data, length, MADV_RANDOM);

  if (!SetIcuDataPointer(icu_data)) {
    PrintIcuNotLoadedWarning();
    return;
  }
  g_initialization_result = true;
}

bool IcuInit() {
  pthread_once(&g_initialization_once, &InitializeIcuDatabase);
  return g_initialization_result;
}
}  // namespace

// Initialize ICU with a static initializer to ensure it is initialized
// before program execution begins. While this is very early, it is not
// guaranteed to be early enough for ICU to be used by other global
// initializers.
static bool g_icu_is_initialized = IcuInit();

}  // namespace icu_init
}  // namespace common
}  // namespace cobalt
