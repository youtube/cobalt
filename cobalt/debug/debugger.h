// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DEBUG_DEBUGGER_H_
#define COBALT_DEBUG_DEBUGGER_H_

#if defined(ENABLE_DEBUG_CONSOLE)

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "cobalt/debug/debug_client.h"
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/debugger_event_target.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {

// Custom interface to communicate with the debugger, modeled after
// chrome.debugger extension API.
// https://developer.chrome.com/extensions/debugger

class Debugger : public script::Wrappable, public DebugClient::Delegate {
 public:
  // JavaScript callback to be run when debugger at/detaches.
  typedef script::CallbackFunction<void()> AttachCallback;
  typedef script::ScriptValue<AttachCallback> AttachCallbackArg;

  // JavaScript callback to be run when a debug command has been executed.
  typedef script::CallbackFunction<void(base::optional<std::string>)>
      CommandCallback;
  typedef script::ScriptValue<CommandCallback> CommandCallbackArg;

  // Callback to be run to get the debug server. The debug server is owned by
  // the web module to which it connects, and this callback allows this object
  // to get a reference to it.
  typedef base::Callback<DebugServer*()> GetDebugServerCallback;

  // Thread-safe ref-counted struct used to pass asynchronously executed
  // command callbacks around. Stores the message loop the callback must be
  // executed on as well as the callback itself.
  struct CommandCallbackInfo
      : public base::RefCountedThreadSafe<CommandCallbackInfo> {
    CommandCallbackInfo(Debugger* const debugger, const CommandCallbackArg& cb)
        : callback(debugger, cb),
          message_loop_proxy(base::MessageLoopProxy::current()) {}
    CommandCallbackArg::Reference callback;
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
    friend class base::RefCountedThreadSafe<CommandCallbackInfo>;
  };

  explicit Debugger(const GetDebugServerCallback& get_debug_server_callback);
  ~Debugger();

  // Non-standard JavaScript extension API.
  void Attach(const AttachCallbackArg& callback);
  void Detach(const AttachCallbackArg& callback);
  void SendCommand(const std::string& method, const std::string& json_params,
                   const CommandCallbackArg& callback);
  const base::optional<std::string>& last_error() const { return last_error_; }
  const scoped_refptr<DebuggerEventTarget>& on_event() const {
    return on_event_;
  }

  DEFINE_WRAPPABLE_TYPE(Debugger);

 protected:
  // Called by the debug server with the response of a command on the message
  // loop the command was sent from (the message loop of this object).
  // Passes the response to the JavaScript callback registered with the command.
  void OnCommandResponse(
      const scoped_refptr<CommandCallbackInfo>& callback_info,
      const base::optional<std::string>& response) const;

  // DebugClient::Delegate implementation.
  void OnDebugClientEvent(
      const std::string& method,
      const base::optional<std::string>& json_params) OVERRIDE;
  void OnDebugClientDetach(const std::string& reason) OVERRIDE;

 private:
  // Runs a script command callback with the specified response.
  // Should be called from the same message loop as the script command that it
  // is a response for.
  void RunCommandCallback(
      const scoped_refptr<CommandCallbackInfo>& callback_info,
      base::optional<std::string> response) const;

  // Callback to be run to get a reference to the debug server.
  GetDebugServerCallback get_debug_server_callback_;

  // Debug client that connects to the server.
  scoped_ptr<DebugClient> debug_client_;

  // This will be defined if there was an error since the last operation.
  // TODO: Chrome implements similar functionality throughout the
  // app using the chrome.runtime.lastError object. When we add support for
  // Cobalt's equivalent to the runtime extension API, we may wish to consider
  // using that and removing this attribute.
  base::optional<std::string> last_error_;

  // Handler for debugger events.
  scoped_refptr<DebuggerEventTarget> on_event_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
#endif  // COBALT_DEBUG_DEBUGGER_H_
