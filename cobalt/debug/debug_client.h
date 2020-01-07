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

#ifndef COBALT_DEBUG_DEBUG_CLIENT_H_
#define COBALT_DEBUG_DEBUG_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace debug {

namespace backend {
// Forward declaration of the dispatcher class that DebugClient can connect to.
class DebugDispatcher;
}  // namespace backend

// An object that can connect to a debug dispatcher. A debug dispatcher can
// accept connections from multiple debug clients, for example to support
// simultaneous connections from the local debug console overlay and a remote
// devtools session.
//
// A DebugClient object can send a command to the attached DebugDispatcher using
// the |SendCommand| method.
//
// Debugging events are handled by forwarding them to a subclass of the
// |DebugClient::Delegate| interface, which must be specified when creating a
// DebugClient.
//
// A DebugDispatcher is owned by the WebModule it attaches to, and may be
// destroyed at any time. When this happens, the DebugDispatcher will notify all
// DebugClient objects via a call to OnDetach. This will nullify the internal
// reference to the DebugDispatcher and notify the DebugClient::Delegate object.
// The DebugClient and DebugDispatcher objects can be running on different
// threads, which makes the use of weak pointers here problematic, so instead
// the |dispatcher_lock_| is used before every access to the dispatcher to
// ensure it is still connected.

class DebugClient {
 public:
  class Delegate {
   public:
    // Event handlers called by the debug dispatcher from its thread. The
    // implementation is responsible for posting the event to its own message
    // loop if necessary.
    virtual void OnDebugClientEvent(const std::string& method,
                                    const std::string& json_params) = 0;

    virtual void OnDebugClientDetach(const std::string& reason) = 0;

   protected:
    virtual ~Delegate();
  };

  // Callback to receive a command response from the dispatcher.
  typedef base::Callback<void(const base::Optional<std::string>& response)>
      ResponseCallback;

  DebugClient(backend::DebugDispatcher* dispatcher, Delegate* delegate);
  ~DebugClient();

  // Whether this client is currently attached to a dispatcher.
  bool IsAttached();

  // Sends a command to the attached dispatcher, with a callback for the
  // response.
  void SendCommand(const std::string& method, const std::string& json_params,
                   const ResponseCallback& callback);

 private:
  friend class backend::DebugDispatcher;

  // Updates the dispatcher when freezing/thawing for navigation.
  void SetDispatcher(backend::DebugDispatcher* dispatcher);

  // Called by the dispatcher when it is destroyed.
  void OnDetach(const std::string& reason);

  // Called by the dispatcher when a debugging event occurs.
  void OnEvent(const std::string& method, const std::string& json_params);

  // No ownership. Access must be protected by |dispatcher_lock_|.
  backend::DebugDispatcher* dispatcher_;

  // No ownership.
  Delegate* delegate_;

  // Used to protect access to the |dispatcher_| member, which may be destroyed
  // from a different thread.
  base::Lock dispatcher_lock_;
};

// Callback to create a DebugClient that's already attached to the main web
// module DebugDispatcher, and sends events to the specified Delegate.
typedef base::Callback<std::unique_ptr<DebugClient>(DebugClient::Delegate*)>
    CreateDebugClientCallback;

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_DEBUG_CLIENT_H_
