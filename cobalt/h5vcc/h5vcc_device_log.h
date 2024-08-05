// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_DEVICE_LOG_H_
#define COBALT_H5VCC_H5VCC_DEVICE_LOG_H_

#include <string>

#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/base/log_message_handler.h"
#include "cobalt/h5vcc/script_callback_wrapper.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccDeviceLog : public script::Wrappable {
 public:
  // The name H5vccDeviceLogHandler can not be changed.
  typedef script::CallbackFunction<void(const std::string&)>
      H5vccDeviceLogHandler;

  typedef ScriptCallbackWrapper<H5vccDeviceLogHandler> DeviceLogHandlerWrapper;
  H5vccDeviceLog() {}

  void OnLog(const DeviceLogHandlerWrapper::ScriptValue& event_handler);

  // Should only be accessed by base::LogMessageHandler.
  bool OnLogInternal(base::Lock* lock, std::string* buffer, int severity,
                     const char* file, int line, size_t message_start,
                     const std::string& str);

  // To be used on the MainWebModule thread.
  void LogToWebClient(std::string message);

  DEFINE_WRAPPABLE_TYPE(H5vccDeviceLog);
  ~H5vccDeviceLog();

 private:
  base::SequencedTaskRunner* task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault().get();
  scoped_refptr<DeviceLogHandlerWrapper> handler_;
  base::LogMessageHandler::CallbackId base_callback_id_ = 0;
  base::Lock lock_;
  std::string buffer_;

  DISALLOW_COPY_AND_ASSIGN(H5vccDeviceLog);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_DEVICE_LOG_H_
