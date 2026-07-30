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

#include <sched.h>

extern "C" {

int __abi_wrap_sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t* mask);
int __abi_wrap_sched_getparam(pid_t pid, struct sched_param* param);
int __abi_wrap_sched_setparam(pid_t pid, const struct sched_param* param);
int __abi_wrap_sched_getscheduler(pid_t pid);
int __abi_wrap_sched_setscheduler(pid_t pid,
                                  int policy,
                                  const struct sched_param* param);

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t* mask) {
  return __abi_wrap_sched_getaffinity(pid, cpusetsize, mask);
}

int sched_getparam(pid_t pid, struct sched_param* param) {
  return __abi_wrap_sched_getparam(pid, param);
}

int sched_setparam(pid_t pid, const struct sched_param* param) {
  return __abi_wrap_sched_setparam(pid, param);
}

int sched_getscheduler(pid_t pid) {
  return __abi_wrap_sched_getscheduler(pid);
}

int sched_setscheduler(pid_t pid, int policy, const struct sched_param* param) {
  return __abi_wrap_sched_setscheduler(pid, policy, param);
}

}  // extern "C"
