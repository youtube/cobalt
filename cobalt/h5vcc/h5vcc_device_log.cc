// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_device_log.h"

#include <sstream>

#include "cobalt/base/cobalt_paths.h"


namespace cobalt {
namespace h5vcc {

void H5vccDeviceLog::OnLog(
    const DeviceLogHandlerWrapper::ScriptValue& event_handler) {
  base::AutoLock auto_lock(lock_);
  task_runner_->RunsTasksInCurrentSequence();
  if (base_callback_id_ != 0) {
    base::LogMessageHandler::GetInstance()->RemoveCallback(base_callback_id_);
  } else {
    auto cb =
        base::Bind(&H5vccDeviceLog::LogToWebClient, base::Unretained(this), "");
    task_runner_->PostTask(FROM_HERE, cb);
  }
  handler_ = new DeviceLogHandlerWrapper(this, event_handler);
  base::LogMessageHandler::OnLogMessageCallback handler_cb = base::Bind(
      &H5vccDeviceLog::OnLogInternal, base::Unretained(this), &lock_, &buffer_);
  base_callback_id_ =
      base::LogMessageHandler::GetInstance()->AddCallback(handler_cb);
}

bool H5vccDeviceLog::OnLogInternal(base::Lock* lock, std::string* buffer,
                                   int severity, const char* file, int line,
                                   size_t message_start,
                                   const std::string& str) {
  std::stringstream ss;
  std::string file_name;
  if (file) {
    file_name = std::string(file);
  }
  ss << "Severity[" << severity << "]:" << file_name << "(" << line << ") "
     << str;
  std::string new_line = ss.str();

  // Unsolved mystery here: accessing any class variables in this
  // callback results in illegal instruction.
  base::AutoLock auto_lock(*lock);
  *buffer += new_line;
}

void H5vccDeviceLog::LogToWebClient(std::string message) {
  base::AutoLock auto_lock(lock_);
  if (handler_ != nullptr && handler_->HasAtLeastOneRef()) {
    handler_->callback.value().Run(buffer_);
    buffer_.clear();
  }
  auto cb =
      base::Bind(&H5vccDeviceLog::LogToWebClient, base::Unretained(this), "");
  task_runner_->PostTask(FROM_HERE, cb);
}

H5vccDeviceLog::~H5vccDeviceLog() {
  CHECK(false);
  base::LogMessageHandler::GetInstance()->RemoveCallback(base_callback_id_);
}

}  // namespace h5vcc
}  // namespace cobalt
