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

#include "base/logging.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "cobalt/shell/android/cobalt_shell_jni_headers/StartupGuard_jni.h"

namespace cobalt {

static void JNI_StartupGuard_OnStartupGuardDisarmed(JNIEnv* env) {
  LOG(INFO) << "StartupGuard disarmed, releasing global startup Persistent "
               "Memory Allocator.";
  base::GlobalHistogramAllocator::ReleaseForTesting();
}

}  // namespace cobalt
