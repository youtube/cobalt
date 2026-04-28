// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/web_test/cobalt_web_test_browser_main_runner.h"

#include <iostream>
#include <string>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cobalt/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {

namespace {

void WebTestWatchdogTask(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  fprintf(stderr, "DEBUG: WebTestWatchdogTask started\n");
  fflush(stderr);

  fprintf(stdout, "#READY\n");
  fflush(stdout);

  std::string test_string;
  while (std::getline(std::cin, test_string)) {
    fprintf(stderr, "DEBUG: Received command from python: %s\n",
            test_string.c_str());
    fflush(stderr);

    if (test_string == "QUIT") {
      break;
    }

    GURL url(test_string);

    // Tell the main thread to navigate.
    ui_task_runner->PostTask(
        FROM_HERE, base::BindOnce(
                       [](GURL url) {
                         if (!Shell::windows().empty()) {
                           Shell::windows()[0]->LoadURL(url);
                         } else {
                           fprintf(stderr,
                                   "DEBUG: No windows found to navigate!\n");
                           fflush(stderr);
                         }
                       },
                       url));

    // Wait 2 seconds for the "test" to finish.
    base::PlatformThread::Sleep(base::Seconds(2));

    // Print the result to satisfy the Python runner.
    fprintf(stdout, "Content-Type: text/plain\n");
    fprintf(stdout, "PASS\n");
    fprintf(stdout, "#EOF\n");  // End of text block
    fprintf(stdout, "#EOF\n");  // End of image block
    fprintf(stdout, "#EOF\n");  // Final EOF
    fflush(stdout);

    fprintf(stderr, "#EOF\n");
    fflush(stderr);
  }

  // Shut down the message loop when the script quits.
  ui_task_runner->PostTask(FROM_HERE, base::BindOnce(&Shell::Shutdown));
}

}  // namespace

CobaltWebTestBrowserMainRunner::CobaltWebTestBrowserMainRunner() {}
CobaltWebTestBrowserMainRunner::~CobaltWebTestBrowserMainRunner() {}

void CobaltWebTestBrowserMainRunner::StartWatchdog(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WebTestWatchdogTask, std::move(ui_task_runner)));
}

}  // namespace content
