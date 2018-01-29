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

#include "base/threading/thread.h"
#include "build/build_config.h"
#include "cobalt/trace_event/benchmark.h"

#if defined(OS_STARBOARD)
#include "starboard/thread.h"
#define usleep(usec) SbThreadSleep(usec)
#endif

// A sample simple benchmark that tracks only a single event, in this case,
// "LoopIteration".
TRACE_EVENT_BENCHMARK1(
    SampleTestBenchmarkWithOneTrackedEvent,
    "LoopIteration", cobalt::trace_event::IN_SCOPE_DURATION) {
  const int kRenderIterationCount = 40;
  for (int i = 0; i < kRenderIterationCount; ++i) {
    TRACE_EVENT0("SampleBenchmark", "LoopIteration");
    {
      TRACE_EVENT0("SampleBenchmark", "SubEventA");
      usleep(10000);
    }
    {
      TRACE_EVENT0("SampleBenchmark", "SubEventB");
      usleep(20000);
    }
  }
}

// This sample benchmark is very similar to the single event tracking benchmark
// above, however it tracks 3 events instead of 1.  For one of those 3 events,
// "SubEventA", we track both its in-scope duration as well as the time between
// subsequent event instance start times.
TRACE_EVENT_BENCHMARK4(
    SampleTestBenchmarkWithThreeTrackedEvents,
    "LoopIteration", cobalt::trace_event::IN_SCOPE_DURATION,
    "SubEventA", cobalt::trace_event::IN_SCOPE_DURATION,
    "SubEventA", cobalt::trace_event::TIME_BETWEEN_EVENT_STARTS,
    "SubEventB", cobalt::trace_event::IN_SCOPE_DURATION) {
  const int kRenderIterationCount = 40;
  for (int i = 0; i < kRenderIterationCount; ++i) {
    TRACE_EVENT0("SampleBenchmark", "LoopIteration");
    {
      TRACE_EVENT0("SampleBenchmark", "SubEventA");
      usleep(10000);
    }
    {
      TRACE_EVENT0("SampleBenchmark", "SubEventB");
      usleep(20000);
    }
  }
}

// The following example demonstrates that the benchmarking system will track
// over flow/thread boundaries.  Since "FlowInitiator"'s duration will always
// include the "HandleTask()" event as a child, "FlowInitiator"'s duration will
// always be greater than "HandleTask()"'s.  In fact, as the worker thread
// gets more and more backed up dealing with HandleTask() tasks (which are
// being produced faster than they can be consumed), the time between when
// "FlowInitiator" starts and "HandleTask()" ends will increase as the
// test continues.
void HandleTask() {
  TRACE_EVENT0("SampleBenchmark", "HandleTask()");
  usleep(10000);
}

TRACE_EVENT_BENCHMARK4(
    FlowExample,
    "FlowInitiator", cobalt::trace_event::IN_SCOPE_DURATION,
    "FlowInitiator", cobalt::trace_event::FLOW_DURATION,
    "FlowInitiator", cobalt::trace_event::TIME_BETWEEN_EVENT_STARTS,
    "HandleTask()", cobalt::trace_event::FLOW_DURATION) {
  base::Thread thread("Worker");
  thread.Start();

  const int kRenderIterationCount = 40;
  for (int i = 0; i < kRenderIterationCount; ++i) {
    usleep(2000);
    TRACE_EVENT0("SampleBenchmark", "FlowInitiator");
    thread.message_loop()->PostTask(FROM_HERE, base::Bind(&HandleTask));
    usleep(5000);
  }
}

// A sample full benchmark where we have the ability to specify the analysis
// method (allowing for sophisticated metrics to be extracted) as well as
// the results compilation method which decides how to present the data
// to subsequent stages of the pipeline.
class SampleTestAdvancedBenchmark : public cobalt::trace_event::Benchmark {
 public:
  // This method will be executed in order to generate the trace results
  // that will subsequently be analyzed.
  void Experiment() override {
    const int kRenderIterationCount = 100;
    for (int i = 0; i < kRenderIterationCount; ++i) {
      TRACE_EVENT0("SampleBenchmark", "LoopIteration");
      {
        TRACE_EVENT0("SampleBenchmark", "SubEventA");
        usleep(10000);
      }
      {
        TRACE_EVENT0("SampleBenchmark", "SubEventB");
        usleep(20000);
      }
    }
  }

  // This method is called to allow the benchmark to handle a trace event
  // occurring.  This occurs when a TRACE_EVENT or TRACE_EVENT_FLOW
  // call from above completes and all its child events and flows have also
  // completed.
  void AnalyzeTraceEvent(
      const scoped_refptr<cobalt::trace_event::EventParser::ScopedEvent>& event)
      override {
    if (event->name() == "LoopIteration") {
      if (event->flow_duration()) {
        loop_iteration_times_in_seconds_.push_back(
            event->flow_duration()->InSecondsF());
      }
    } else if (event->name() == "SubEventA") {
      if (event->flow_duration()) {
        sub_event_a_times_in_seconds_.push_back(
            event->flow_duration()->InSecondsF());
      }
    } else if (event->name() == "SubEventB") {
      if (event->flow_duration()) {
        sub_event_b_times_in_seconds_.push_back(
            event->flow_duration()->InSecondsF());
      }
    }
  }

  // This is called after all events have been analyzed and it is time to
  // produce results.  All Result objects returned will be output as separate
  // variables.
  std::vector<Result> CompileResults() override {
    std::vector<Result> results;

    results.push_back(Result("LoopIteration time in seconds",
                             loop_iteration_times_in_seconds_));
    results.push_back(
        Result("SubEventA time in seconds", sub_event_a_times_in_seconds_));
    results.push_back(
        Result("SubEventB time in seconds", sub_event_b_times_in_seconds_));

    return results;
  }

 private:
  std::vector<double> loop_iteration_times_in_seconds_;
  std::vector<double> sub_event_a_times_in_seconds_;
  std::vector<double> sub_event_b_times_in_seconds_;
};
TRACE_EVENT_REGISTER_BENCHMARK(SampleTestAdvancedBenchmark);
