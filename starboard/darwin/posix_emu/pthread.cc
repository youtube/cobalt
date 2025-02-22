// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>

#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

#include "starboard/configuration_constants.h"

typedef int (*func_pthread_setname_np_t)(const char* name);

static func_pthread_setname_np_t g_ptr_set_name =
    reinterpret_cast<func_pthread_setname_np_t>(
        dlsym(RTLD_NEXT, "pthread_setname_np"));

extern "C" {

int pthread_setname_np(pthread_t thread, const char* name) {
  if (pthread_equal(thread, pthread_self())) {
    char buffer[kSbMaxThreadNameLength];

    if (strlen(name) >= SB_ARRAY_SIZE_INT(buffer)) {
      starboard::strlcpy(buffer, name, SB_ARRAY_SIZE_INT(buffer));
      SB_LOG(WARNING) << "Thread name \"" << name << "\" was truncated to \""
                      << buffer << "\"";
      name = buffer;
    }

    if ((*g_ptr_set_name)(name) != 0) {
      SB_LOG(ERROR) << "Failed to set thread name to " << name;
      return -1;
    }
    return 0;
  } else {
    SB_LOG(ERROR) << "Failed to set thread name to " << name
                  << " for different thread";
    return -1;
  }
}

}  // extern "C"
