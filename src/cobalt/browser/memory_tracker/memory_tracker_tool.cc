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

#include "cobalt/browser/memory_tracker/memory_tracker_tool.h"

#include <cstdlib>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/browser/memory_tracker/memory_tracker_tool_impl.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "nb/lexical_cast.h"
#include "starboard/log.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
// Null implementation for memory tracker tool.
class MemoryTrackerToolNull : public MemoryTrackerTool {
 public:
  virtual ~MemoryTrackerToolNull() {}
};

// Instantiates the memory tracker tool from the command argument.
scoped_ptr<MemoryTrackerTool> CreateMemoryTrackerTool(const std::string&) {
  return scoped_ptr<MemoryTrackerTool>(new MemoryTrackerToolNull);
}

#else

using nb::analytics::MemoryTracker;

namespace {
enum SwitchEnum {
  kNull,
  kStartup,
  kContinuousPrinter,
  kCompressedTimeseries,
  kBinnerAnalytics,
  kAllocationLogger,
  kLeakTracer,
  kJavascriptLeakTracer,
};

struct SwitchVal {
  SwitchVal() : help_msg(), enum_value(kNull) {}
  SwitchVal(const SwitchVal& other)
      : tool_name(other.tool_name),
        help_msg(other.help_msg),
        enum_value(other.enum_value) {}
  SwitchVal(const char* name, const char* msg, SwitchEnum val)
      : tool_name(name), help_msg(msg), enum_value(val) {}

  std::string tool_name;
  std::string help_msg;
  SwitchEnum enum_value;
};

class SbLogger : public AbstractLogger {
 public:
  virtual void Output(const char* str) OVERRIDE { SbLogRaw(str); }
  virtual void Flush() OVERRIDE { SbLogFlush(); }
};

// Parses out the toolname for a tool command.
// Example:
//   INPUT:  "tool_name(arg)"
//   OUTPUT: "tool_name"
std::string ParseToolName(const std::string& command_arg) {
  const size_t first_open_paren_idx = command_arg.find('(');
  if (first_open_paren_idx == std::string::npos) {
    return command_arg;
  }
  return command_arg.substr(0, first_open_paren_idx);
}

// Parses out the arguments for a tool.
// Example:
//   INPUT:  "tool_name(arg)"
//   OUTPUT: "arg"
std::string ParseToolArg(const std::string& command_arg) {
  const size_t first_open_paren_idx = command_arg.find('(');
  const size_t last_closing_paren_idx = command_arg.find_last_of(')');

  if ((first_open_paren_idx == std::string::npos) ||
     (last_closing_paren_idx == std::string::npos)) {
    return "";
  }

  if (last_closing_paren_idx < first_open_paren_idx) {
    return "";
  }

  const size_t begin_idx = first_open_paren_idx+1;
  const size_t length = (last_closing_paren_idx-begin_idx);
  std::string arg_str = command_arg.substr(begin_idx, length);
  return arg_str;
}
}  // namespace.

class MemoryTrackerThreadImpl : public MemoryTrackerTool {
 public:
  explicit MemoryTrackerThreadImpl(base::SimpleThread* ptr)
      : thread_ptr_(ptr) {}
  virtual ~MemoryTrackerThreadImpl() OVERRIDE { thread_ptr_.reset(NULL); }
  scoped_ptr<base::SimpleThread> thread_ptr_;
};

scoped_ptr<MemoryTrackerTool> CreateMemoryTrackerTool(
    const std::string& command_arg) {
  // The command line switch for memory_tracker was used. Look into the args
  // and determine the mode that the memory_tracker should be used for.

  // The map is used to
  // 1) Resolve the string to an enum.
  // 2) Print a useful help method of all known memory_tracker modes.
  typedef std::map<std::string, SwitchVal> SwitchMap;

  SwitchVal startup_tool(
      "startup(num_mins=1)",  // Name of tool.
      "Records high-frequency memory metrics for the first 60 "
      "seconds of program launch and then dumps it out in CSV format "
      "to stdout.",
      kStartup);

  SwitchVal continuous_printer_tool(
      "continuous_printer",  // Name of tool.
      "Once every second the memory state is dumped to stdout.",
      kContinuousPrinter);

  SwitchVal compressed_timeseries_tool(
      "compressed_timeseries",  // Name of tool.
      "Use this tool to see the growth in memory usage as the app runs. The "
      "memory growth is segmented into memory scopes and outputted as CSV. "
      "The compressed time-series will depict the full history of the memory "
      "using a fixed number of rows. Older history has degraded resolution and "
      "while new entries are captured in full detail. This achieved by "
      "evicting old entries by an exponential decay scheme.",
      kCompressedTimeseries);

  SwitchVal binner_tool(
      "binner(region=NULL)",
      "Dumps memory statistics once a second in CSV format to stdout. "
      "The default memory region is all memory regions. Pass the "
      "name of the memory region to specify that only that memory region "
      "should be tracked. For example: binner(Javascript).",
      kBinnerAnalytics);

  SwitchVal allocation_logger_tool(
      "allocation_logger",
      "Continuously writes allocations and deallocations to memory_log.txt. "
      "This is a legacy format used by lbshell. The location of this "
      "memory_log.txt file is in the platform dependent directory specified "
      "by kSbSystemPathDebugOutputDirectory.",
      kAllocationLogger);

  SwitchVal leak_tracing_tool(
      "leak_tracer",
      "Automatically detects leaks and reports them in CSV format.",
      kLeakTracer);

  SwitchVal js_leak_tracing_tool(
      "js_leak_tracer",
      "Automatically detects Javascript leaks and reports them in CSV format.",
      kJavascriptLeakTracer);

  SwitchMap switch_map;
  switch_map[ParseToolName(startup_tool.tool_name)] = startup_tool;
  switch_map[ParseToolName(continuous_printer_tool.tool_name)] =
      continuous_printer_tool;
  switch_map[ParseToolName(compressed_timeseries_tool.tool_name)] =
      compressed_timeseries_tool;
  switch_map[ParseToolName(binner_tool.tool_name)] = binner_tool;
  switch_map[ParseToolName(allocation_logger_tool.tool_name)] =
      allocation_logger_tool;
  switch_map[ParseToolName(leak_tracing_tool.tool_name)] = leak_tracing_tool;
  switch_map[ParseToolName(js_leak_tracing_tool.tool_name)] =
      js_leak_tracing_tool;

  std::string tool_name = ParseToolName(command_arg);
  std::string tool_arg = ParseToolArg(command_arg);

  // FAST OUT - is a thread type not selected? Then print out a help menu
  // and request that the app should shut down.
  SwitchMap::const_iterator found_it = switch_map.find(tool_name);
  if (found_it == switch_map.end()) {
    // Error, tell the user what to do instead and then exit.
    std::stringstream ss;
    ss << "\nNo memory tracker tool selected so help has been invoked:\n";
    ss << "Memory Tracker help:\n";
    for (SwitchMap::const_iterator it = switch_map.begin();
         it != switch_map.end(); ++it) {
      const std::string& name = it->first;
      const SwitchVal& val = it->second;
      ss << "    memory_tracker=" << name << " "
         << "\"" << val.help_msg << "\"\n";
    }
    ss << "\n";
    SbLogRaw(ss.str().c_str());
    ss.str("");  // Clears the contents of stringstream.
    SbLogFlush();

    // Try and turn off all logging so that the stdout is less likely to be
    // polluted by interleaving output.
    logging::SetMinLogLevel(INT_MAX);
    // SbThreadSleep wants microseconds. We sleep here so that the user can
    // read the help message before the engine starts up again and the
    // fills the output with more logging text.
    const SbTime four_seconds = 4000 * kSbTimeMillisecond;
    SbThreadSleep(four_seconds);

    found_it = switch_map.find(continuous_printer_tool.tool_name);

    ss << "Defaulting to tool: " << found_it->first << "\n";
    SbLogRaw(ss.str().c_str());
    SbLogFlush();
    // One more help message and 1-second pause. Then continue on with the
    // execution as normal.
    const SbTime one_second = 1000 * kSbTimeMillisecond;
    SbThreadSleep(one_second);
  }

  // Okay we have been resolved to use a memory tracker in some way.
  DLOG(INFO) << "\n\nUsing MemoryTracking=" << found_it->first << "\n";

  // Tools are expected to instantiate the MemoryTracker if they need it.
  MemoryTracker* memory_tracker = NULL;
  scoped_ptr<AbstractMemoryTrackerTool> tool_ptr;

  const SwitchVal& value = found_it->second;
  switch (value.enum_value) {
    case kNull: {
      SB_NOTREACHED();
      break;
    }
    case kStartup: {
      double num_mins = 1.0;
      if (!tool_arg.empty()) {
        num_mins = nb::lexical_cast<double>(tool_arg.c_str());
        if ((num_mins > 0) == false) {  // Accounts for NaN.
          num_mins = 1.0;
        }
      }
      memory_tracker = MemoryTracker::Get();
      memory_tracker->InstallGlobalTrackingHooks();

      // Converts minutes into Sampling interval and also total sampling time.
      // To keep the amount of elements in the output static, we adjust the
      // sampling.
      //
      // The number of samples produced is held constants and is:
      //   kNumSamples = F::TotalSamplingTime(num_mins) /
      //                 F::SamplingIntervalMs(num_mins)
      struct F {
        static int SamplingIntervalMs(double mins) {
          // kNumSamples is chosen such that SamplingIntervalMs(1) outputs
          // 240ms.
          static const double kNumSamples = 250.;
          const int sample_time_ms =
              static_cast<int>(ToMilliseconds(mins) / kNumSamples);
          return static_cast<int>(sample_time_ms);
        }

        // TotalSamplingTime(1) outputs 60,000 milliseconds, or 1 minute.
        static int ToMilliseconds(double mins) {
          const double millseconds = mins * 60. * 1000.;
          return static_cast<int>(millseconds);
        }
      };

      // Sample time increases with the number of seconds. At one second we
      // sample 4 times a second. This keeps the number of samples constant
      // regardless of input time.
      int sampling_interval_ms = F::SamplingIntervalMs(num_mins);
      // Time until output is triggered.
      int sampling_time_ms = F::ToMilliseconds(num_mins);
      // Create a thread that will gather memory metrics for startup.
      tool_ptr.reset(
          new MemoryTrackerPrintCSV(sampling_interval_ms, sampling_time_ms));
      break;
    }
    case kContinuousPrinter: {
      memory_tracker = MemoryTracker::Get();
      memory_tracker->InstallGlobalTrackingHooks();
      // Create a thread that will continuously report memory use.
      tool_ptr.reset(new MemoryTrackerPrint);
      break;
    }
    case kCompressedTimeseries: {
      memory_tracker = MemoryTracker::Get();
      memory_tracker->InstallGlobalTrackingHooks();
      // Create a thread that will continuously report memory use.
      tool_ptr.reset(new MemoryTrackerCompressedTimeSeries);
      break;
    }
    case kBinnerAnalytics: {
      memory_tracker = MemoryTracker::Get();
      memory_tracker->InstallGlobalTrackingHooks();
      // Create a thread that will continuously report javascript memory
      // analytics.
      tool_ptr.reset(new MemorySizeBinner(tool_arg));
      break;
    }
    case kAllocationLogger: {
      scoped_ptr<MemoryTrackerLogWriter> disk_writer_tool(
          new MemoryTrackerLogWriter());
      tool_ptr.reset(disk_writer_tool.release());
      break;
    }
    case kLeakTracer: {
      scoped_ptr<MemoryTrackerLeakFinder> leak_finder(
          new MemoryTrackerLeakFinder(MemoryTrackerLeakFinder::kCPlusPlus));

      memory_tracker = MemoryTracker::Get();
      memory_tracker->InstallGlobalTrackingHooks();
      memory_tracker->SetMemoryTrackerDebugCallback(leak_finder.get());
      tool_ptr.reset(leak_finder.release());
      break;
    }
    case kJavascriptLeakTracer: {
      scoped_ptr<MemoryTrackerLeakFinder> leak_finder(
          new MemoryTrackerLeakFinder(MemoryTrackerLeakFinder::kJavascript));

      memory_tracker = MemoryTracker::Get();
      memory_tracker->InstallGlobalTrackingHooks();
      memory_tracker->SetMemoryTrackerDebugCallback(leak_finder.get());
      tool_ptr.reset(leak_finder.release());
      break;
    }
    default: {
      SB_NOTREACHED() << "Unhandled case.";
      break;
    }
  }

  if (tool_ptr.get()) {
    base::SimpleThread* thread =
        new MemoryTrackerToolThread(memory_tracker,  // May be NULL.
                                    tool_ptr.release(), new SbLogger);
    return scoped_ptr<MemoryTrackerTool>(new MemoryTrackerThreadImpl(thread));
  } else {
    return scoped_ptr<MemoryTrackerTool>();
  }
}

#endif  // !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
