// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/linux/shared/command_line_defaults.h"

namespace starboard {
namespace linux_platform {  // name conflict w/ compiler macro "linux"
namespace shared {

namespace {
// See: cobalt/browser/switches.cc
const char kDevServersListenIpSwitch[] = "dev_servers_listen_ip";
const char kDevServersListenIpSwitchDefault[] = "::1";
}  // namespace

using starboard::shared::starboard::CommandLine;

CommandLine GetCommandLine(int argc, char** argv) {
  CommandLine command_line(argc, argv);

  // On Linux we default dev servers to only listen to localhost.
  if (!command_line.HasSwitch(kDevServersListenIpSwitch)) {
    command_line.AppendSwitch(kDevServersListenIpSwitch,
                              kDevServersListenIpSwitchDefault);
  }

  return command_line;
}

}  // namespace shared
}  // namespace linux_platform
}  // namespace starboard
