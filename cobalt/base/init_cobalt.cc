/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/base/init_cobalt.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "cobalt/deprecated/platform_delegate.h"
#if !defined(OS_STARBOARD)
#include "lbshell/src/lb_memory_manager.h"
#endif

namespace cobalt {
namespace {

void AtExitCallback(void* /*param*/) {
  deprecated::PlatformDelegate::Teardown();
}

#if LB_ENABLE_MEMORY_DEBUGGING && !defined(OS_STARBOARD)

// Define some functions that will be used by the memory log writer system which
// does not have access to base::Time.

base::LazyInstance<base::Time> s_program_start_time = LAZY_INSTANCE_INITIALIZER;

void SetupProgramStartTime() {
  s_program_start_time.Get() = base::Time::Now();
}

uint32 GetLifetimeInMS() {
  return static_cast<uint32>(
      (base::Time::Now() - s_program_start_time.Get()).InMilliseconds());
}

#endif

}  // namespace

void InitCobalt(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  deprecated::PlatformDelegate::Init();

  // Register a callback to be called during program termination.
  // This will fail if AtExitManager wasn't created before calling InitCobalt.
  base::AtExitManager::RegisterCallback(&AtExitCallback, NULL);

#if LB_ENABLE_MEMORY_DEBUGGING && !defined(OS_STARBOARD)
  if (LB::Memory::IsContinuousLogEnabled()) {
    SetupProgramStartTime();
    // Initialize the writer (we should already be recording to memory) now that
    // the filesystem and threading system are initialized.
    lb_memory_init_log_writer(&GetLifetimeInMS);
  }
#endif

  bool icu_initialized = icu_util::Initialize();
  LOG_IF(ERROR, !icu_initialized) << "ICU initialization failed.";
}

}  // namespace cobalt
