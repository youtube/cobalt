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

#include <sys/resource.h>

extern "C" {

int __abi_wrap_getpriority(int which, id_t who);

int getpriority(int which, id_t who) {
  return __abi_wrap_getpriority(which, who);
}

int __abi_wrap_getrlimit(int resource, struct rlimit* rlp);

int getrlimit(int resource, struct rlimit* rlp) {
  return __abi_wrap_getrlimit(resource, rlp);
}

int __abi_wrap_setpriority(int which, id_t who, int prio);

int setpriority(int which, id_t who, int prio) {
  return __abi_wrap_setpriority(which, who, prio);
}
}
