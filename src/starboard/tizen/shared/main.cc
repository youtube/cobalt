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


#include <time.h>
#include <stdio.h>

#include "starboard/configuration.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/suspend_signals.h"

void main2()
{
  printf("GDB Test - main2\n");
}

void main3()
{
  printf("GDB Test - main3\n");
}

int main(int argc, char** argv) {
  printf("Hello Cobalt!\n");
  main2();
  main3();
  tzset();
  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  return 0;
}
