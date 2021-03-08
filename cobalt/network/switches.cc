// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/switches.h"

namespace cobalt {
namespace network {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
// Log network events using Chromium's network logger. If a parameter is
// specified, the parameter shall be interpreted as a file path and the network
// log will be written into the file. Otherwise, logging will be written to
// VLOG(1).
const char kNetLog[] = "net_log";

// Corresponds to the capture mode described in the NetLogCaptureMode defined
// in net/log/net_log_capture_mode.h. Except for the default mode, Cobalt also
// takes in "IncludeCookiesAndCredentials" and "IncludeSocketBytes" modes.
const char kNetLogCaptureMode[] = "net_log_capture_mode";

const char kUserAgent[] = "user_agent";

const char kMaxNetworkDelay[] = "max_network_delay";
const char kMaxNetworkDelayHelp[] =
    "Add an artificial random delay (up to the value specified) before the "
    "start of network requests to simulate low-latency networks.  The value "
    "is specified in microseconds.";
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

// Switch to disable use of the Quic network protocol.
const char kDisableQuic[] = "disable_quic";

}  // namespace switches
}  // namespace network
}  // namespace cobalt
