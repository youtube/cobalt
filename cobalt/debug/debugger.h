/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef DEBUG_DEBUGGER_H_
#define DEBUG_DEBUGGER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/debug_server.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {

// Custom interface to communicate with the debugger, loosely modeled after
// chrome.debugger extension API.
// https://developer.chrome.com/extensions/debugger

class Debugger : public script::Wrappable {
 public:
  // JavaScript callback to be run when debugger at/detaches.
  typedef script::CallbackFunction<void()> AttachCallback;
  typedef script::ScriptObject<AttachCallback> AttachCallbackArg;

  // JavaScript callback to be run when a debug command has been executed.
  typedef script::CallbackFunction<void(
      scoped_refptr<script::Wrappable> result)> CommandCallback;
  typedef script::ScriptObject<CommandCallback> CommandCallbackArg;

  // Callback to be run to create a debug server.
  typedef base::Callback<scoped_ptr<script::DebugServer>(void)>
      CreateDebugServerCallback;

  explicit Debugger(
      const CreateDebugServerCallback& create_debug_server_callback);
  ~Debugger();

  // Non-standard JavaScript extension API.
  void Attach(const AttachCallbackArg& callback);
  void Detach(const AttachCallbackArg& callback);
  void SendCommand(const std::string& method,
                   scoped_refptr<script::Wrappable> params,
                   const CommandCallbackArg& callback);

  DEFINE_WRAPPABLE_TYPE(Debugger);

 private:
  // Callback to be run to create a debug server.
  CreateDebugServerCallback create_debug_server_callback_;

  // Reference to the debug server that implements the functionality.
  scoped_ptr<script::DebugServer> debug_server_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // DEBUG_DEBUGGER_H_
