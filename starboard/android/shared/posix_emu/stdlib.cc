// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {

int __real_mkostemp(char* tmpl, int flags);

int __wrap_mkostemp(char* tmpl, int flags) {
  // Bionic only accepts O_APPEND|O_CLOEXEC|O_DSYNC|O_RSYNC|O_SYNC here and
  // supplies O_RDWR|O_CREAT|O_EXCL itself, while musl callers pass those in.
  // Drop them; any other flag musl would forward to open() is still rejected.
  flags &= ~(O_ACCMODE | O_CREAT | O_EXCL);
  return __real_mkostemp(tmpl, flags);
}

}  // extern "C"
