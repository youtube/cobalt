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

#include <cstdio>
#include <map>

#include "base/command_line.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/browser/application.h"
#include "cobalt/browser/switches.h"

namespace {

// How many seconds to wait after starting the application before taking the
// snapshot of CVal values.
const int kSecondsToWait = 20;

// The list of CVals we are interested in reporting.
//
// Note that if you add an item to this list, you should also add a column
// to the stats tracking table in the database. Please see the document titled
// "Adding a SnapshotAppStats column" on the Cobalt intranet home page for
// examples on how to add a new column.

// The naming convention for CVal columns in the database are the same as the
// name of the CVal except with the dots removed and the CVal name converted to
// camel case.  For example, "DOM.TokenLists" would become "dOMTokenLists".
//
// A document that explains how to modify the database in more detail can be
// found called "Dashboard TDD" on the Cobalt intranet home page.

const char* g_cvals_to_snapshot[] = {
    "Count.DOM.Nodes",
    "Count.DOM.TokenLists",
    "Count.XHR",
    "Memory.ArrayBuffer",
    "Memory.CPU.Exe",
    "Memory.CPU.Used",
    "Memory.GraphicsPS3.Fixed.Size",
    "Memory.JS",
    "Memory.MainWebModule.ImageCache.Size",
    "Memory.MainWebModule.RemoteTypefaceCache.Size",
    "Memory.Media.AudioDecoder",
    "Memory.Media.MediaSource.CPU.Fixed.Capacity",
    "Memory.Media.VideoDecoder",
    "Memory.XHR",
};

PRINTF_FORMAT(1, 2) void Output(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  std::vfprintf(stdout, fmt, ap);

  va_end(ap);

  std::fflush(stdout);
}

typedef std::map<std::string, std::string> CValsMap;

// Returns all CVals along with their values.
CValsMap GetAllCValValues() {
  base::CValManager* cvm = base::CValManager::GetInstance();
  std::set<std::string> cvals = cvm->GetOrderedCValNames();
  CValsMap cvals_with_values;

  for (std::set<std::string>::iterator iter = cvals.begin();
       iter != cvals.end(); ++iter) {
    base::Optional<std::string> cval_value = cvm->GetValueAsString(*iter);
    if (cval_value) {
      cvals_with_values[*iter] = *cval_value;
    }
  }

  return cvals_with_values;
}

// Grab a snapshot of all current CVals and their values and then output them
// so that they can be analyzed by humans and inserted into a database where
// the values can be graphed.
void DoStatsSnapshot(cobalt::browser::Application* application) {
  Output("---Benchmark Results Start---\n");
  Output("{\n");
  Output("  \"LiveYouTubeAfter%dSecondsStatsSnapshot\": {\n", kSecondsToWait);

  CValsMap cval_values = GetAllCValValues();
  bool have_printed_results = false;
  for (size_t i = 0; i < arraysize(g_cvals_to_snapshot); ++i) {
    CValsMap::iterator found = cval_values.find(g_cvals_to_snapshot[i]);
    if (found != cval_values.end()) {
      if (have_printed_results) {
        Output(",\n");
      } else {
        have_printed_results = true;
      }
      Output("    \"%s\": %s", found->first.c_str(), found->second.c_str());
    }
  }

  Output("\n");

  Output("  }\n");
  Output("}\n");
  Output("---Benchmark Results End---\n");

  application->Quit();
}

}  // namespace

namespace {

cobalt::browser::Application* g_application = NULL;
base::Thread* g_snapshot_thread = NULL;

void StartApplication(int argc, char* argv[], const char* link,
                      const base::Closure& quit_closure,
                      SbTimeMonotonic timestamp) {
  logging::SetMinLogLevel(100);

  // Use null storage for our savegame so that we don't persist state from
  // one run to another, which makes the measurements more deterministic (e.g.
  // we will not consistently register for experiments via cookies).
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      cobalt::browser::switches::kNullSavegame);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      cobalt::browser::switches::kDebugConsoleMode, "off");

  // Create the application object just like is done in the Cobalt main app.
  g_application =
      new cobalt::browser::Application(quit_closure, false /*should_preload*/,
                                       timestamp);

  // Create a thread to start a timer for kSecondsToWait seconds after which
  // we will take a snapshot of the CVals at that time and then quit the
  // application.
  g_snapshot_thread = new base::Thread("StatsSnapshot");
  g_snapshot_thread->Start();
  g_snapshot_thread->message_loop()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&DoStatsSnapshot, g_application),
      base::TimeDelta::FromSeconds(kSecondsToWait));
}

void StopApplication() {
  g_snapshot_thread->Stop();
  delete g_application;
  g_application = NULL;
}

}  // namespace

COBALT_WRAP_BASE_MAIN(StartApplication, StopApplication);
