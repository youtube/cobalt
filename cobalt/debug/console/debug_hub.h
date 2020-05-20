// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_CONSOLE_DEBUG_HUB_H_
#define COBALT_DEBUG_CONSOLE_DEBUG_HUB_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "cobalt/debug/console/console_command.h"
#include "cobalt/debug/console/debug_console_mode.h"
#include "cobalt/debug/console/debugger_event_target.h"
#include "cobalt/debug/debug_client.h"
#include "cobalt/dom/c_val_view.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {
namespace console {

// This class implements an interface to JavaScript for debugging.
// The main (and typically only) JavaScript client is the debug console.
//
// This is modeled after chrome.debugger extension API.
// https://developer.chrome.com/extensions/debugger
class DebugHub : public script::Wrappable, public DebugClient::Delegate {
 public:
  // Function signature to call when we need to query for the Hud visibility
  // mode.
  typedef base::Callback<debug::console::DebugConsoleMode()> GetHudModeCallback;

  // JavaScript callback to be run when debugger attaches/detaches.
  typedef script::CallbackFunction<void()> AttachCallback;
  typedef script::ScriptValue<AttachCallback> AttachCallbackArg;

  // JavaScript callback to receive the response after executing a command.
  typedef script::CallbackFunction<void(base::Optional<std::string>)>
      ResponseCallback;
  typedef script::ScriptValue<ResponseCallback> ResponseCallbackArg;

  // Thread-safe ref-counted struct used to pass asynchronously executed
  // response callbacks around. Stores the message loop the callback must be
  // executed on as well as the callback itself.
  struct ResponseCallbackInfo
      : public base::RefCountedThreadSafe<ResponseCallbackInfo> {
    ResponseCallbackInfo(DebugHub* const debugger,
                         const ResponseCallbackArg& cb)
        : callback(debugger, cb),
          task_runner(base::MessageLoop::current()->task_runner()) {}
    ResponseCallbackArg::Reference callback;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    friend class base::RefCountedThreadSafe<ResponseCallbackInfo>;
  };

  DebugHub(const GetHudModeCallback& get_hud_mode_callback,
           const CreateDebugClientCallback& create_debug_client_callback);
  ~DebugHub();

  const scoped_refptr<dom::CValView>& c_val() const { return c_val_; }

  debug::console::DebugConsoleMode GetDebugConsoleMode() const;

  void Attach(const AttachCallbackArg& callback);
  void Detach(const AttachCallbackArg& callback);

  // Read a text file from the content/web/debug_console/ directory and return
  // its contents.
  std::string ReadDebugContentText(const std::string& filename);

  // Sends a devtools protocol command to be executed in the context of the main
  // WebModule that is being debugged.
  void SendCommand(const std::string& method, const std::string& json_params,
                   const ResponseCallbackArg& callback);

  const base::Optional<std::string>& last_error() const { return last_error_; }
  const scoped_refptr<DebuggerEventTarget>& on_event() const {
    return on_event_;
  }

  const script::Sequence<ConsoleCommand> console_commands() const;

  // Sends a console command to be handled in the context of the debug WebModule
  // by a registered hander. This lets the JavaScript debug console trigger
  // actions in the app.
  void SendConsoleCommand(const std::string& command,
                          const std::string& message);

  DEFINE_WRAPPABLE_TYPE(DebugHub);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  // Called by the debug dispatcher with the response of a command on the
  // message loop the command was sent from (the message loop of this object).
  // Passes the response to the JavaScript callback registered with the command.
  void OnCommandResponse(
      const scoped_refptr<ResponseCallbackInfo>& callback_info,
      const base::Optional<std::string>& response) const;

  // DebugClient::Delegate implementation.
  void OnDebugClientEvent(const std::string& method,
                          const std::string& json_params) override;
  void OnDebugClientDetach(const std::string& reason) override;

 private:
  // Runs a script response callback with the specified response.
  // Should be called from the same message loop as the script command that it
  // is a response for.
  void RunResponseCallback(
      const scoped_refptr<ResponseCallbackInfo>& callback_info,
      base::Optional<std::string> response) const;

  // A view onto Cobalt's CVals
  scoped_refptr<dom::CValView> c_val_;

  // A function to query the Hud visibility mode.
  const GetHudModeCallback get_hud_mode_callback_;

  // A factory function to create a debug client.
  const CreateDebugClientCallback create_debug_client_callback_;

  // Handler for debugger events.
  scoped_refptr<DebuggerEventTarget> on_event_;

  // Debug client that connects to the dispatcher.
  std::unique_ptr<DebugClient> debug_client_;

  // This will be defined if there was an error since the last operation.
  // TODO: Chrome implements similar functionality throughout the
  // app using the chrome.runtime.lastError object. When we add support for
  // Cobalt's equivalent to the runtime extension API, we may wish to consider
  // using that and removing this attribute.
  base::Optional<std::string> last_error_;
};

}  // namespace console
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_CONSOLE_DEBUG_HUB_H_
