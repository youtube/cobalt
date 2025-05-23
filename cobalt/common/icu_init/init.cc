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

void PrintIcuNotLoadedWarning() {
  SB_LOG(WARNING) << "ICU Database could not be loaded!";
}

// Initialize the ICU using the file named kIcuDataFileName in the
// kSbSystemPathContentDirectory folder. Note that this gives the ICU its
// database, but it does not actually set a default timezone or locale.
void InitializeIcuDatabase() {
  std::vector<char> base_path(kSbFileMaxPath);
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, base_path.data(),
                                base_path.size());
  SB_DCHECK(result);
  std::string data_path(base_path.data());
  data_path += kSbFileSepString;
  data_path += kIcuDataFileName;

  const char* path = data_path.c_str();

  struct stat st;
  int stat_result = stat(path, &st);
  if (stat_result != 0 || st.st_size <= 0) {
    SB_DCHECK(stat_result != -1) << "Failed to stat ICU data " << data_path
                                 << ", error " << strerror(errno);
    SB_DCHECK(stat_result == 0 && st.st_size > 0)
        << "ICU data file unexpectedly has zero length.";
    PrintIcuNotLoadedWarning();
    return;
  }
  off_t length = st.st_size;

  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    SB_DCHECK(fd != -1) << "Failed to stat ICU data " << data_path << ", error "
                        << strerror(errno);
    PrintIcuNotLoadedWarning();
    return;
  }
  void* icu_data = mmap(0, length, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);

  if (!icu_data || icu_data == MAP_FAILED) {
    SB_DCHECK(icu_data != MAP_FAILED) << "Failed to mmap ICU data " << data_path
                                      << ", error " << strerror(errno);
    SB_DCHECK(icu_data) << "Null pointer returned trying to mmap ICU data "
                        << data_path;
    PrintIcuNotLoadedWarning();
    return;
  }

  madvise(icu_data, length, MADV_RANDOM);

  UErrorCode error_code = U_ZERO_ERROR;
  udata_setCommonData(icu_data, &error_code);
  if (U_FAILURE(error_code)) {
    SB_DCHECK(U_SUCCESS(error_code))
        << "Failed to set ICU common data pointer, error "
        << u_errorName(error_code);
    PrintIcuNotLoadedWarning();
  }
}

bool IcuInit() {
  pthread_once(&g_initialization_once, &InitializeIcuDatabase);
  return true;
}
}  // namespace

// Initialize ICU with a static initializer to ensure it is initialized
// before program execution begins.
static bool g_icu_is_initialized = IcuInit();

}  // namespace icu_init
}  // namespace common
}  // namespace cobalt
