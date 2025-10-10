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

#include <stdarg.h>
#include <sys/prctl.h>

extern "C" {

int __abi_wrap_prctl(int op, ...);

int prctl(int op, ...) {
  unsigned long args[4];
  va_list ap;
  va_start(ap, op);
  int ret = __abi_wrap_prctl(op, ap);
  va_end(ap);
  return ret;
}
}
