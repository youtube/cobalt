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

#ifndef COBALT_DEBUG_DEBUG_CLIENT_H_
#define COBALT_DEBUG_DEBUG_CLIENT_H_

#include <string>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/debug/debug_server.h"

namespace cobalt {
namespace debug {

// An object that can connect to a debug server. A debug server can accept
// connections from multiple debug clients, for example to support simultaneous
// connections from the local debug console and a remote devtools session.
//
// A DebugClient object can send a command to the attached DebugServer using the
// |SendCommand| method.
//
// Debugging events are handled by creating a subclass of the
// DebugClient::Delegate class, an instance of which must be specified when
// creating a DebugClient.
//
// A DebugServer is owned to the WebModule it attaches to, and may be destroyed
// at any time. When this happens, the DebugServer will notify all
// DebugClient objects via a call to OnDetach. This will nullify the internal
// reference to the DebugServer and notify the DebugClient::Delegate object.
// The DebugClient and DebugServer objects can be running on different threads,
// which makes the use of weak pointers here problematic, so instead the
// |server_lock_| is used before every access to the server to ensure it is
// still connected.

class DebugClient {
 public:
  class Delegate {
   public:
    // Event handlers called by the debug server from its thread.
    virtual void OnDebugClientEvent(
        const std::string& method,
        const base::optional<std::string>& json_params) = 0;

    virtual void OnDebugClientDetach(const std::string& reason) = 0;

   protected:
    virtual ~Delegate();
  };

  DebugClient(DebugServer* server, Delegate* delegate);
  ~DebugClient();

  // Whether this client is currently attached to a server.
  bool IsAttached();

  // Called by the server when it is destroyed.
  void OnDetach(const std::string& reason);

  // Called by the server when a debugging event occurs.
  void OnEvent(const std::string& method,
               const base::optional<std::string>& json_params);

  // Sends a command to the attached server, with a callback for the response.
  void SendCommand(const std::string& method, const std::string& json_params,
                   const DebugServer::CommandCallback& callback);

 private:
  // No ownership. Access must be protected by |server_lock_|.
  DebugServer* server_;

  // No ownership.
  Delegate* delegate_;

  // Used to protect access to the |server_| member, which may be destroyed
  // from a different thread.
  base::Lock server_lock_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_DEBUG_CLIENT_H_
