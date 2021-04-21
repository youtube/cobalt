// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <cmath>

#include "cobalt/extension/cwrappers.h"
#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/system.h"

static double (*g_pow_wrapper)(double, double) = &pow;
static SbOnceControl g_pow_wrapper_once = SB_ONCE_INITIALIZER;

static void InitPowWrapper(void) {
  const CobaltExtensionCWrappersApi* cwrappers =
      (const CobaltExtensionCWrappersApi*)SbSystemGetExtension(
          kCobaltExtensionCWrappersName);
  if (cwrappers != NULL && cwrappers->version >= 1 &&
      cwrappers->PowWrapper != NULL) {
    g_pow_wrapper = cwrappers->PowWrapper;
  }
}

extern "C" {

double CobaltPowWrapper(double base, double exponent) {
  SbOnce(&g_pow_wrapper_once, InitPowWrapper);
  return g_pow_wrapper(base, exponent);
}

}
