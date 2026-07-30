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

#include "chrome/updater/constants.h"
#include "chrome/updater/updater.h"
#include "starboard/common/log.h"
#include "starboard/event.h"

namespace {
int g_argc = 0;
char** g_argv = nullptr;
}  // namespace

void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    updater::UpdaterMain(g_argc, g_argv);
  }
}

#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
int main(int argc, char* argv[]) {
  g_argc = argc;
  g_argv = argv;

  SB_LOG(INFO) << "Number of arguments (argc): " << g_argc;
  for (int i = 0; i < g_argc; ++i) {
    SB_LOG(INFO) << "argv[" << i << "]: " << g_argv[i];
  }

  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
