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

#include <cstdio>
#include <map>

#include "base/at_exit.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "cobalt/base/console_values.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/browser/application.h"

namespace {

// How many seconds to wait after starting the application before taking the
// snapshot of CVal values.
const int kSecondsToWait = 20;

// The list of CVals we are interested in reporting.
const char* g_cvals_to_snapshot[] = {
    "DOM.Nodes",
    "DOM.TokenLists",
    "DOM.XHR",
    "MainWebModule.ImageCache.Used",
    "MainWebModule.RemoteFontCache.Used",
    "Memory.CPU.Exe",
    "Memory.CPU.Used",
    "Memory.GraphicsPS3.Fixed.size",
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
  base::ConsoleValueManager* cvm = base::ConsoleValueManager::GetInstance();
  std::set<std::string> cvals = cvm->GetOrderedCValNames();
  CValsMap cvals_with_values;

  for (std::set<std::string>::iterator iter = cvals.begin();
       iter != cvals.end(); ++iter) {
    base::optional<std::string> cval_value = cvm->GetValueAsString(*iter);
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
  Output("---Results Start Here---\n");
  Output("{\n");
  Output("  \"LiveKabukiAfter%dSecondsStatsSnapshot\": {\n", kSecondsToWait);

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

  application->Quit();
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);

  logging::SetMinLogLevel(100);

  // Create the application object just like is done in the Cobalt main app.
  scoped_ptr<cobalt::browser::Application> application =
      cobalt::browser::CreateApplication();

  // Create a thread to start a timer for kSecondsToWait seconds after which
  // we will take a snapshot of the CVals at that time and then quit the
  // application.
  base::Thread snapshot_thread("StatsSnapshot");
  snapshot_thread.Start();
  snapshot_thread.message_loop()->PostDelayedTask(
      FROM_HERE, base::Bind(&DoStatsSnapshot, application.get()),
      base::TimeDelta::FromSeconds(kSecondsToWait));

  application->Run();

  snapshot_thread.Stop();

  return 0;
}
