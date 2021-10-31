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

#include "cobalt/debug/console/debug_hub.h"

#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/debug/console/command_manager.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {
namespace console {

namespace {
constexpr char kContentDir[] = "debug_console";
}  // namespace

DebugHub::DebugHub(
    const GetHudModeCallback& get_hud_mode_callback,
    const CreateDebugClientCallback& create_debug_client_callback)
    : c_val_(new dom::CValView()),
      get_hud_mode_callback_(get_hud_mode_callback),
      create_debug_client_callback_(create_debug_client_callback),
      on_event_(new DebuggerEventTarget()) {}

DebugHub::~DebugHub() {}

debug::console::DebugConsoleMode DebugHub::GetDebugConsoleMode() const {
  return get_hud_mode_callback_.Run();
}

void DebugHub::Attach(const AttachCallbackArg& callback) {
  last_error_ = base::nullopt;
  debug_client_ = create_debug_client_callback_.Run(this);

  // |debug_client_| may be NULL if the WebModule is not available at this time.
  if (!debug_client_) {
    DLOG(WARNING) << "Debug dispatcher unavailable.";
    last_error_ = "Debug dispatcher unavailable.";
  }

  AttachCallbackArg::Reference callback_reference(this, callback);
  callback_reference.value().Run();
}

void DebugHub::Detach(const AttachCallbackArg& callback) {
  last_error_ = base::nullopt;
  debug_client_.reset();
  AttachCallbackArg::Reference callback_reference(this, callback);
  callback_reference.value().Run();
}

std::string DebugHub::ReadDebugContentText(const std::string& filename) {
  std::string result;

  base::FilePath file_path;
  base::PathService::Get(paths::DIR_COBALT_WEB_ROOT, &file_path);
  file_path = file_path.AppendASCII(kContentDir);
  file_path = file_path.AppendASCII(filename);

  std::string text;
  if (!base::ReadFileToString(file_path, &text)) {
    DLOG(WARNING) << "Cannot read file: " << file_path.value();
  }
  return text;
}

void DebugHub::SendCommand(const std::string& method,
                           const std::string& json_params,
                           const ResponseCallbackArg& callback) {
  last_error_ = base::nullopt;
  if (!debug_client_ || !debug_client_->IsAttached()) {
    std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue);
    response->SetString("error.message", "Debugger is not connected.");
    std::string json_response;
    auto* response_as_value = static_cast<const base::Value*>(response.get());
    base::JSONWriter::Write(*response_as_value, &json_response);
    ResponseCallbackArg::Reference callback_ref(this, callback);
    callback_ref.value().Run(json_response);
    return;
  }

  scoped_refptr<ResponseCallbackInfo> callback_info(
      new ResponseCallbackInfo(this, callback));
  debug_client_->SendCommand(
      method, json_params,
      base::Bind(&DebugHub::OnCommandResponse, this, callback_info));
}

const script::Sequence<ConsoleCommand> DebugHub::console_commands() const {
  script::Sequence<ConsoleCommand> result;
  ConsoleCommandManager* command_manager = ConsoleCommandManager::GetInstance();
  DCHECK(command_manager);
  if (command_manager) {
    std::set<std::string> commands = command_manager->GetRegisteredCommands();
    for (std::set<std::string>::const_iterator it = commands.begin();
         it != commands.end(); ++it) {
      ConsoleCommand console_command;
      console_command.set_command(*it);
      console_command.set_short_help(command_manager->GetShortHelp(*it));
      console_command.set_long_help(command_manager->GetLongHelp(*it));
      result.push_back(console_command);
    }
  }
  return result;
}

void DebugHub::SendConsoleCommand(const std::string& command,
                                  const std::string& message) {
  ConsoleCommandManager* console_command_manager =
      ConsoleCommandManager::GetInstance();
  DCHECK(console_command_manager);
  console_command_manager->HandleCommand(command, message);
}

void DebugHub::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(on_event_);
}

void DebugHub::OnCommandResponse(
    const scoped_refptr<ResponseCallbackInfo>& callback_info,
    const base::Optional<std::string>& response) const {
  // Run the script callback on the message loop the command was sent from.
  callback_info->task_runner->PostTask(
      FROM_HERE, base::Bind(&DebugHub::RunResponseCallback, this, callback_info,
                            response));
}

void DebugHub::OnDebugClientEvent(const std::string& method,
                                  const std::string& params) {
  // Pass to the onEvent handler. The handler will notify the JavaScript
  // listener on the message loop the listener was registered on.
  on_event_->DispatchEvent(method, params);
}

void DebugHub::OnDebugClientDetach(const std::string& reason) {
  DLOG(INFO) << "Debugger detached: " + reason;
  const std::string method = "Inspector.detached";
  JSONObject params(new base::DictionaryValue());
  params->SetString("reason", reason);
  on_event_->DispatchEvent(method, JSONStringify(params));
}

void DebugHub::RunResponseCallback(
    const scoped_refptr<ResponseCallbackInfo>& callback_info,
    base::Optional<std::string> response) const {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            callback_info->task_runner);
  callback_info->callback.value().Run(std::move(response));
}

}  // namespace console
}  // namespace debug
}  // namespace cobalt
