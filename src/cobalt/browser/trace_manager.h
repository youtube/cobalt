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

#ifndef COBALT_BROWSER_TRACE_MANAGER_H_
#define COBALT_BROWSER_TRACE_MANAGER_H_

#include <map>
#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/base/console_commands.h"
#include "cobalt/trace_event/event_parser.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace browser {

class TraceManager {
 public:
  // Returns whether there's a trace that is active.
  static bool IsTracing();

  TraceManager();

  // Called by browser module when a key event is produced.
  void OnKeyEventProduced();

  // Message handler callback for trace message from debug console.
  void OnTraceMessage(const std::string& message);

  // Message handler callback for key trace message from debug console.
  void OnKeyTraceMessage(const std::string& message);

  // Called when receiving and finishing receiving parsed trace events.
  void OnReceiveTraceEvent(
      const scoped_refptr<trace_event::EventParser::ScopedEvent>& event);
  void OnFinishReceiveTraceEvent();

 private:
  typedef std::multimap<int64,
                        scoped_refptr<trace_event::EventParser::ScopedEvent> >
      StartTimeToEventMap;

  // The message loop on which we'll do all our work.
  MessageLoop* const self_message_loop_;

  // Command handler object for trace command from the debug console.
  base::ConsoleCommandManager::CommandHandler trace_command_handler_;
  // Command handler object for key trace command from the debug console.
  base::ConsoleCommandManager::CommandHandler key_trace_command_handler_;
  // Whether key tracing is enabled.
  bool key_tracing_enabled_;

  base::ThreadChecker thread_checker_;
  // This object can be set to start a trace if a hotkey (like F3) is pressed.
  // While initialized, it means that a trace is on-going.
  scoped_ptr<trace_event::ScopedTraceToFile> trace_to_file_;
  // Record of a list of events we're interested in, ordered by starting time.
  StartTimeToEventMap start_time_to_event_map_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_TRACE_MANAGER_H_
