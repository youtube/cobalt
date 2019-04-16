// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/init_cobalt.h"

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/path_provider.h"

namespace cobalt {
namespace {

base::LazyInstance<std::string>::DestructorAtExit::DestructorAtExit
    s_initial_deep_link = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InitCobalt(int argc, char* argv[], const char* link) {
  base::CommandLine::Init(argc, argv);
  if (link) {
    s_initial_deep_link.Get() = link;
  }
  bool icu_initialized = base::i18n::InitializeICU();
  LOG_IF(ERROR, !icu_initialized) << "ICU initialization failed.";

  // Register a path provider for Cobalt-specific paths.
  base::PathService::RegisterProvider(&PathProvider, paths::PATH_COBALT_START,
                                      paths::PATH_COBALT_END);
}

const char* GetInitialDeepLink() { return s_initial_deep_link.Get().c_str(); }

}  // namespace cobalt
