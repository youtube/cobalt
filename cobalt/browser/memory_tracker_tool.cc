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

#include "cobalt/browser/memory_tracker_tool.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

#if defined(OS_STARBOARD)
#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_impl.h"
#include "nb/scoped_ptr.h"
#include "nb/simple_thread.h"
#endif

namespace cobalt {
namespace browser {

#if !defined(OS_STARBOARD)
// A dummy implementation.
scoped_ptr<MemoryTrackerTool> CreateMemoryTrackerTool(const std::string&) {
  DLOG(INFO)
      << "Memory tracker tool is not enabled on non-starboard builds.";
  MemoryTrackerTool* null_ptr = NULL;
  return scoped_ptr<MemoryTrackerTool>(null_ptr);
}

#else  // defined(OS_STARBOARD)

namespace {
enum SwitchEnum {
  kNull,
  kStartup,
  kContinuousPrinter,
  kCompressedTimeseries,
  kJavascriptMemoryAnalytics,
};

struct SwitchVal {
  SwitchVal() : help_msg(), enum_value(kNull) {}
  SwitchVal(const SwitchVal& other)
    : tool_name(other.tool_name), help_msg(other.help_msg),
      enum_value(other.enum_value) {}
  SwitchVal(const char* name, const char* msg, SwitchEnum val)
    : tool_name(name), help_msg(msg), enum_value(val) {}

  std::string tool_name;
  std::string help_msg;
  SwitchEnum enum_value;
};

}  // namespace.

class MemoryTrackerThreadImpl : public MemoryTrackerTool {
 public:
  explicit MemoryTrackerThreadImpl(nb::SimpleThread* ptr)
    : thread_ptr_(ptr) {
  }

  virtual ~MemoryTrackerThreadImpl() OVERRIDE {}
  scoped_ptr<nb::SimpleThread> thread_ptr_;
};

scoped_ptr<MemoryTrackerTool> CreateMemoryTrackerTool(
    const std::string& command_arg) {
  // The command line switch for memory_tracker was used. Look into the args
  // and determine the mode that the memory_tracker should be used for.
  using nb::analytics::MemoryTrackerPrintThread;
  using nb::analytics::MemoryTracker;

  // The map is used to
  // 1) Resolve the string to an enum.
  // 2) Print a useful help method of all known memory_tracker modes.
  typedef std::map<std::string, SwitchVal> SwitchMap;

  SwitchVal startup_tool(
    "startup",  // Name of tool.
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
    "while new entries are captured in full detail. This achieved by evicting "
    "old entries by an exponential decay scheme.",
    kCompressedTimeseries);

  SwitchVal javascript_memory_tool(
    "javascript",
    "Dumps javascript memory statistics once a second in CSV format to "
    "stdout.",
    kJavascriptMemoryAnalytics);

  SwitchMap switch_map;
  switch_map[startup_tool.tool_name] = startup_tool;
  switch_map[continuous_printer_tool.tool_name] = continuous_printer_tool;
  switch_map[compressed_timeseries_tool.tool_name] =
    compressed_timeseries_tool;

  // FAST OUT - is a thread type not selected? Then print out a help menu
  // and request that the app should shut down.
  SwitchMap::const_iterator found_it = switch_map.find(command_arg);
  if (found_it == switch_map.end()) {
    // Error, tell the user what to do instead and then exit.
    std::stringstream ss;
    ss << "\nNo memory tracker tool selected so help has been invoked:\n";
    ss << "Memory Tracker help:\n";
    for (SwitchMap::const_iterator it = switch_map.begin();
      it != switch_map.end(); ++it) {
        const std::string& name = it->first;
        const SwitchVal& val = it->second;
        ss << "    memory_tracker="
          << name << " " << "\"" << val.help_msg << "\"\n";
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

  MemoryTracker* memory_tracker = MemoryTracker::Get();
  memory_tracker->InstallGlobalTrackingHooks();

  using nb::analytics::MemoryTrackerPrintCSVThread;
  using nb::analytics::MemoryTrackerPrintThread;
  using nb::analytics::MemoryTrackerCompressedTimeSeriesThread;
  using nb::analytics::JavascriptMemoryTrackerThread;

  scoped_ptr<nb::SimpleThread> thread_ptr;
  const SwitchVal& value = found_it->second;
  switch (value.enum_value) {
    case kNull: {
      SB_NOTREACHED();
      break;
    }
    case kStartup: {
      // Create a thread that will gather memory metrics for startup.
      thread_ptr.reset(
          new MemoryTrackerPrintCSVThread(
              memory_tracker,
              1000,          // Sample once a second.
              60* 1000));    // Output after 60 seconds.
      break;
    }
    case kContinuousPrinter: {
      // Create a thread that will continuously report memory use.
      thread_ptr.reset(
          new MemoryTrackerPrintThread(memory_tracker));
      break;
    }
    case kCompressedTimeseries: {
      // Create a thread that will continuously report memory use.
      thread_ptr.reset(
          new MemoryTrackerCompressedTimeSeriesThread(memory_tracker));
      break;
    }
    case kJavascriptMemoryAnalytics: {
      // Create a thread that will continuously report javascript memory
      // analytics.
      thread_ptr.reset(
          new JavascriptMemoryTrackerThread(memory_tracker));
      break;
    }
    default: {
      SB_NOTREACHED() << "Unhandled case.";
      break;
    }
  }
  // Wrap the tool up in MemoryTrackerThreadImpl.
  return scoped_ptr<MemoryTrackerTool>(
      new MemoryTrackerThreadImpl(thread_ptr.release()));
}

#endif  // defined(OS_STARBOARD)

}  // namespace browser
}  // namespace cobalt
