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

#include <sys/time.h>
#include <time.h>

extern "C" {

int __abi_wrap_clock_gettime(clockid_t clk_id, struct timespec* ts);

int clock_gettime(clockid_t clk_id, struct timespec* ts) {
  return __abi_wrap_clock_gettime(clk_id, ts);
}

int __abi_wrap_gettimeofday(struct timeval* tp, void* tzp);

int gettimeofday(struct timeval* tp, void* tzp) {
  return __abi_wrap_gettimeofday(tp, tzp);
}

time_t __abi_wrap_time(time_t* tloc);

time_t time(time_t* tloc) {
  return __abi_wrap_time(tloc);
}

struct tm* __abi_wrap_gmtime_r(const time_t* clock, struct tm* result);

struct tm* gmtime_r(const time_t* clock, struct tm* result) {
  return __abi_wrap_gmtime_r(clock, result);
}
}
