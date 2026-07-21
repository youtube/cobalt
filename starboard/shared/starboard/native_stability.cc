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

#include "starboard/shared/starboard/native_stability.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/extension/native_stability.h"

namespace starboard {

namespace {

ReadReportsCallback g_read_reports_callback = nullptr;
pthread_mutex_t g_read_reports_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

int ReadReports(SbNativeStabilityReport* reports, int max_num_reports) {
  int result = -1;

  SB_CHECK_EQ(pthread_mutex_lock(&g_read_reports_callback_mutex), 0);
  if (g_read_reports_callback) {
    result = g_read_reports_callback(reports, max_num_reports);
  }
  SB_CHECK_EQ(pthread_mutex_unlock(&g_read_reports_callback_mutex), 0);

  return result;
}

void RegisterReadReportsCallback(ReadReportsCallback callback) {
  SB_CHECK_EQ(pthread_mutex_lock(&g_read_reports_callback_mutex), 0);
  g_read_reports_callback = callback;
  SB_CHECK_EQ(pthread_mutex_unlock(&g_read_reports_callback_mutex), 0);
}

const CobaltExtensionNativeStabilityApi kNativeStabilityApi = {
    kCobaltExtensionNativeStabilityName,
    1,
    &ReadReports,
    &RegisterReadReportsCallback,
};

}  // namespace

const void* GetNativeStabilityApi() {
  return &kNativeStabilityApi;
}

}  // namespace starboard
