// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/system.h"
// clang-format on

// Adapted from base/rand_util_posix.cc

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include "starboard/common/check_op.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"

namespace {

// We keep the open file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive).
class URandomFile {
 public:
  URandomFile() {
    file_ = open("/dev/urandom", O_RDONLY);
    SB_DCHECK(starboard::IsValid(file_)) << "Cannot open /dev/urandom";
  }

  ~URandomFile() { close(file_); }

  int file() const { return file_; }

 private:
  int file_;
};

// A file that will produce any number of very random bytes.
URandomFile* g_urandom_file = NULL;

// Control to initialize g_urandom_file.
pthread_once_t g_urandom_file_once = PTHREAD_ONCE_INIT;

// Lazily initialize g_urandom_file.
void InitializeRandom() {
  SB_DCHECK_EQ(g_urandom_file, nullptr);
  g_urandom_file = new URandomFile();
}

}  // namespace

void SbSystemGetRandomData(void* out_buffer, int buffer_size) {
  SB_DCHECK(out_buffer);
  char* buffer = reinterpret_cast<char*>(out_buffer);
  int remaining = buffer_size;
  [[maybe_unused]] int once_result =
      pthread_once(&g_urandom_file_once, &InitializeRandom);
  SB_DCHECK_EQ(once_result, 0);

  int file = g_urandom_file->file();
  do {
    // This is unsynchronized access to the File that could happen from multiple
    // threads. It doesn't appear that there is any locking in the Chromium
    // POSIX implementation that is very similar.
    int result = read(file, buffer, remaining);
    if (result <= 0) {
      break;
    }

    remaining -= result;
    buffer += result;
  } while (remaining);

  SB_CHECK_EQ(remaining, 0);
}
