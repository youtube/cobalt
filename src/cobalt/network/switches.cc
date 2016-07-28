/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/network/switches.h"

namespace cobalt {
namespace network {
namespace switches {

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
// Log network events using Chromium's network logger. If a parameter is
// specified, the parameter shall be interpreted as a file path and the network
// log will be written into the file. Otherwise, logging will be written to
// VLOG(1).
const char kNetLog[] = "net_log";

// Corresponds to the levels described in the NetLog::LogLevel enum defined in
// net/base/net_log.h
const char kNetLogLevel[] = "net_log_level";
#endif  // ENABLE_COMMAND_LINE_SWITCHES

}  // namespace switches
}  // namespace network
}  // namespace cobalt
