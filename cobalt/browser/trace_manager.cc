// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/trace_manager.h"

#include "base/compiler_specific.h"
#include "base/debug/trace_event_impl.h"
#include "base/message_loop.h"
#include "cobalt/trace_event/scoped_event_parser_trace.h"

namespace cobalt {
namespace browser {
namespace {

// Name of the channel to listen for trace commands from the debug console.
const char kTraceCommandChannel[] = "trace";

// Help strings for the trace command channel.
const char kTraceCommandShortHelp[] = "Starts/stops execution tracing.";
const char kTraceCommandLongHelp[] =
    "If a trace is currently running, stops it and saves the result; "
    "otherwise starts a new trace.";

// Name of the command to start / stop tracing after input event.
const char kInputTraceCommand[] = "input_trace";

// Help strings for the input trace command.
const char kInputTraceCommandShortHelp[] =
    "Starts/stops tracing after input event";
const char kInputTraceCommandLongHelp[] =
    "Switches the flag of whether we start a new tracing after each input "
    "event.";

}  // namespace

bool TraceManager::IsTracing() {
  return base::debug::TraceLog::GetInstance()->IsEnabled();
}

TraceManager::TraceManager()
    : self_message_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(trace_command_handler_(
          kTraceCommandChannel,
          base::Bind(&TraceManager::OnTraceMessage, base::Unretained(this)),
          kTraceCommandShortHelp, kTraceCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(input_trace_command_handler_(
          kInputTraceCommand,
          base::Bind(&TraceManager::OnInputTraceMessage,
                     base::Unretained(this)),
          kInputTraceCommandShortHelp, kInputTraceCommandLongHelp)),
      input_tracing_enabled_(false) {}

void TraceManager::OnInputEventProduced() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (input_tracing_enabled_ && !IsTracing()) {
    static const int kTraceTimeInMilliSeconds = 500;
    LOG(INFO) << "Input event produced, start tracing for "
              << kTraceTimeInMilliSeconds << "ms...";
    start_time_to_event_map_.clear();
    trace_event::TraceWithEventParserForDuration(
        base::Bind(&TraceManager::OnReceiveTraceEvent, base::Unretained(this)),
        base::Bind(&TraceManager::OnFinishReceiveTraceEvent,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(kTraceTimeInMilliSeconds));
  }
}

void TraceManager::OnTraceMessage(const std::string& message) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&TraceManager::OnTraceMessage,
                                            base::Unretained(this), message));
    return;
  }

  DCHECK(thread_checker_.CalledOnValidThread());

  static const char* kOutputTraceFilename = "triggered_trace.json";

  if (trace_to_file_) {
    LOG(INFO) << "Ending trace.";
    LOG(INFO) << "Trace results in file \""
              << trace_to_file_->absolute_output_path().value() << "\"";
    trace_to_file_.reset();
  } else {
    if (IsTracing()) {
      LOG(WARNING)
          << "Cannot manually trigger a trace when another trace is active.";
    } else {
      LOG(INFO) << "Starting trace...";
      trace_to_file_.reset(
          new trace_event::ScopedTraceToFile(FilePath(kOutputTraceFilename)));
    }
  }
}

void TraceManager::OnInputTraceMessage(const std::string& message) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&TraceManager::OnInputTraceMessage,
                                            base::Unretained(this), message));
    return;
  }

  DCHECK(thread_checker_.CalledOnValidThread());

  input_tracing_enabled_ = !input_tracing_enabled_;
  LOG(INFO) << "Input tracing is now "
            << (input_tracing_enabled_ ? "enabled" : "disabled") << ".";
}

void TraceManager::OnReceiveTraceEvent(
    const scoped_refptr<trace_event::EventParser::ScopedEvent>& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO: Generalize the following logic. Currently the criteria for
  // interesting events are hardcoded.
  if (event->name() == "WebModule::InjectKeyboardEvent()" ||
      event->name() == "WebModule::InjectPointerEvent()" ||
      event->name() == "WebModule::InjectWheelEvent()" ||
      event->name() == "Layout") {
    double event_duration = event->in_scope_duration()->InMillisecondsF();

    if (event->name() == "Layout") {
      static const double kTrivialLayoutThresholdInMilliSeconds = 10;
      if (event_duration < kTrivialLayoutThresholdInMilliSeconds) {
        return;
      }
    }

    start_time_to_event_map_.insert(std::make_pair(
        event->begin_event().timestamp().ToInternalValue(), event));
  }
}

void TraceManager::OnFinishReceiveTraceEvent() {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (StartTimeToEventMap::iterator it = start_time_to_event_map_.begin();
       it != start_time_to_event_map_.end(); ++it) {
    trace_event::EventParser::ScopedEvent* event = it->second;
    LOG(INFO) << "  " << event->name() << " "
              << event->in_scope_duration()->InMillisecondsF() << "ms";
  }
  start_time_to_event_map_.clear();
  LOG(INFO) << "Input trace finished.";
}

}  // namespace browser
}  // namespace cobalt
