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

#include <map>
#include <string>

namespace cobalt {
namespace network {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
const char kNetLog[] = "net_log";
const char kNetLogHelp[] =
    "Log network events using Chromium's network logger. If a parameter is"
    "specified,"
    "the parameter shall be interpreted as a file path and the network log"
    "will be written into the file.Otherwise,"
    "logging will be written to VLOG(1).";

const char kNetLogCaptureMode[] = "net_log_capture_mode";
const char kNetLogCaptureModeHelp[] =
    "Corresponds to the capture mode described in the NetLogCaptureMode"
    "defined in net/log/net_log_capture_mode.h. Except for the default"
    "mode, Cobalt also takes in |IncludeCookiesAndCredentials| and"
    "|IncludeSocketBytes| modes.";

const char kUserAgent[] = "user_agent";
const char kUserAgentHelp[] =
    "Specifies a custom user agent for device simulations. The expected "
    "format is \"Mozilla/5.0 ('os_name_and_version') Cobalt/'cobalt_version'."
    "'cobalt_build_version_number'-'build_configuration' (unlike Gecko) "
    "'javascript_engine_version' 'rasterizer_type' 'starboard_version', "
    "'network_operator'_'device_type'_'chipset_model_number'_'model_year'/"
    "'firmware_version' ('brand', 'model', 'connection_type') 'aux_field'\".";

const char kMaxNetworkDelay[] = "max_network_delay";
const char kMaxNetworkDelayHelp[] =
    "Add an artificial random delay (up to the value specified) before the "
    "start of network requests to simulate low-latency networks.  The value "
    "is specified in microseconds.";

const char kDisableInAppDial[] = "disable_in_app_dial";
const char kDisableInAppDialHelp[] = "Disable the in-app dial server.";

const char kQuicConnectionOptions[] = "quic_connection_options";
const char kQuicConnectionOptionsHelp[] =
    "Specify QUIC connection options. "
    "Refer to QUICHE header crypto_protocol.h for existing tags. "
    "For example: --quic_connection_options=AKDU";

const char kQuicClientConnectionOptions[] = "quic_client_connection_options.";
const char kQuicClientConnectionOptionsHelp[] =
    "Specify QUIC client connection options"
    "Refer to QUICHE header crypto_protocol.h for existing tags. ";

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

const char kDisableQuic[] = "disable_quic";
const char kDisableQuicHelp[] =
    "Switch to disable use of the Quic network protocol.";

const char kDisableHttp2[] = "disable_h2";
const char kDisableHttp2Help[] =
    "Switch to disable use of the HTTP/2 (SPDY) network protocol.";

std::map<std::string, const char*> HelpMap() {
  std::map<std::string, const char*> help_map{
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      {kNetLog, kNetLogHelp},
      {kNetLogCaptureMode, kNetLogCaptureModeHelp},
      {kUserAgent, kUserAgentHelp},
      {kMaxNetworkDelay, kMaxNetworkDelayHelp},
      {kDisableInAppDial, kDisableInAppDialHelp},
      {kQuicConnectionOptions, kQuicConnectionOptionsHelp},
      {kQuicClientConnectionOptions, kQuicClientConnectionOptionsHelp},
#endif  // !defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      {kDisableQuic, kDisableQuicHelp},
      {kDisableHttp2, kDisableHttp2Help},
  };
  return help_map;
}


}  // namespace switches
}  // namespace network
}  // namespace cobalt
