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

#ifndef COBALT_BASE_INIT_COBALT_H_
#define COBALT_BASE_INIT_COBALT_H_

namespace cobalt {

// Initializes Cobalt-related things in the binary. This includes initializing
// command line flags and various low-level platform-specific systems.
//
// Before calling this function, at least one base::AtExitManager must be
// created.
//
// Example:
// int main(int argc, char* argv[]) {
//   base::AtExitManager at_exit;
//   cobalt::InitCobalt(argc, argv);
//
//   ..RunSomeCode..
// }
void InitCobalt(int argc, char* argv[], const char* initial_deep_link);

// Get the |initial_deep_link| string specified in |InitCobalt|.
const char* GetInitialDeepLink();

}  // namespace cobalt

#endif  // COBALT_BASE_INIT_COBALT_H_
