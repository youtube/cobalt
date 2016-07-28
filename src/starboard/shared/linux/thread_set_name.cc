// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/thread.h"

#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/log.h"
#include "starboard/string.h"

void SbThreadSetName(const char* name) {
  // We don't want to rename the main thread.
  if (SbThreadGetId() == getpid()) {
    return;
  }

  const int kMaxThreadNameLength = 16;
  char buffer[kMaxThreadNameLength];

  if (SbStringGetLength(name) >= SB_ARRAY_SIZE_INT(buffer)) {
    SbStringCopy(buffer, name, SB_ARRAY_SIZE_INT(buffer));
    SB_DLOG(WARNING) << "Thread name \"" << name << "\" was truncated to \""
                     << buffer << "\"";
    name = buffer;
  }

  if (pthread_setname_np(pthread_self(), name) != 0) {
    SB_DLOG(ERROR) << "Failed to set thread name to " << name;
  }
}
