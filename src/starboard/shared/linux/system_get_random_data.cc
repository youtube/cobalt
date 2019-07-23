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

// Adapted from base/rand_util_posix.cc

#include "starboard/system.h"

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/file.h"
#include "starboard/once.h"

namespace {

// We keep the open file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive).
class URandomFile {
 public:
  URandomFile() {
    file_ =
        SbFileOpen("/dev/urandom", kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    SB_DCHECK(SbFileIsValid(file_)) << "Cannot open /dev/urandom";
  }

  ~URandomFile() { SbFileClose(file_); }

  SbFile file() const { return file_; }

 private:
  SbFile file_;
};

// A file that will produce any number of very random bytes.
URandomFile* g_urandom_file = NULL;

// Control to initialize g_urandom_file.
SbOnceControl g_urandom_file_once = SB_ONCE_INITIALIZER;

// Lazily initialize g_urandom_file.
void InitializeRandom() {
  SB_DCHECK(g_urandom_file == NULL);
  g_urandom_file = new URandomFile();
}

}  // namespace

void SbSystemGetRandomData(void* out_buffer, int buffer_size) {
  SB_DCHECK(out_buffer);
  char* buffer = reinterpret_cast<char*>(out_buffer);
  int remaining = buffer_size;
  bool once_result = SbOnce(&g_urandom_file_once, &InitializeRandom);
  SB_DCHECK(once_result);

  SbFile file = g_urandom_file->file();
  do {
    // This is unsynchronized access to the File that could happen from multiple
    // threads. It doesn't appear that there is any locking in the Chromium
    // POSIX implementation that is very similar.
    int result = SbFileRead(file, buffer, remaining);
    if (result <= 0)
      break;

    remaining -= result;
    buffer += result;
  } while (remaining);

  SB_CHECK(remaining == 0);
}
