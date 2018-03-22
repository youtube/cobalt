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
// NetLog is an optional component that allows socket access to starboard
// logs. Installing this component requires that the platform starboard
// implementation call NetLogWrite(...) from the LogRaw(...) and
// LogRawFormat(...) implementations.
//
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

#ifndef STARBOARD_SHARED_STARBOARD_NET_LOG_H_
#define STARBOARD_SHARED_STARBOARD_NET_LOG_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/socket.h"

namespace starboard {
namespace shared {
namespace starboard {

// Command line switch useful for determining if NetLogWaitForClientConnected()
// should be called.
extern const char kNetLogCommandSwitchWait[];

// Optional - Pauses execution of the current thread until
// a client has connected.
void NetLogWaitForClientConnected();

// Writes to the netlog buffer socket stream.
// Note that the NetLog is completely disabled for official
// builds, and the resources will be lazily created on the first
// call.
void NetLogWrite(const char* data);

// If no client is connected, then this will be a no-op.
// Otherwise blocks until all the pending data is written to the currently
// connected client.
void NetLogFlush();

// Optional and may be called during shutdown, this will cause all pending
// data in the Netlog to be flushed before returning. After this call
// the NetLog is effectively disabled and cannot be re-enabled for the life
// of the process. Calling multiple times is safe.
void NetLogFlushThenClose();

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_NET_LOG_H_
