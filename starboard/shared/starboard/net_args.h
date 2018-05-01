// Copyright 2018 Google Inc. All Rights Reserved.
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
//
// NetArgs is an optional component that allows socket access to tell an
// executable what it's command args are.
//
// ****** //
// TODO: Remove the following:
// It's recommended to call NetLogWaitForClientConnected() to ensure that the
// NetLog is running a predictable time from launch. It's also STRONGLY
// recommended that NetLogFlushThenClose() is called during shutdown to
// guarantee that any pending data is flushed out through the network layer.
// Failure to do this will often result in the netlog being truncated as the
// process shuts down.
//
// The netlog create a server that will bind to ip 0.0.0.0 (and similar in
// ipv6) and will listen for client connections. See net_log.cc for which
// port net_log will listen to.
//
// See also net_log.py for an example script that can connect to a running
// netlog instance.

#ifndef STARBOARD_SHARED_STARBOARD_NET_ARGS_H_
#define STARBOARD_SHARED_STARBOARD_NET_ARGS_H_

#include <string>
#include <vector>

namespace starboard {
namespace shared {
namespace starboard {

// Command line switch useful for determining if NetArgsWaitForConnection()
// should be called.
extern const char kNetArgsCommandSwitchWait[];

// Waits until args stream in with a socket connection and data reception.
std::vector<std::string> NetArgsWaitForPayload();

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_NET_ARGS_H_
