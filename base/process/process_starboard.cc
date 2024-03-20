// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <cstdlib>

#include "base/process/process.h"
#include "base/process/process_handle.h"

namespace {
base::ProcessId kStarboardFakeProcessId = 1;
}

namespace base {

ProcessId GetProcId(ProcessHandle) {
  return kStarboardFakeProcessId;
}

ProcessId GetParentProcessId(ProcessHandle) {
  return kStarboardFakeProcessId;
}

ProcessId GetCurrentProcId() {
  return kStarboardFakeProcessId;
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

bool Process::IsProcessBackgrounded() const {
  return false;
}

Time Process::CreationTime() const {
  return Time();
}

#ifndef STARBOARD
// static
void Process::TerminateCurrentProcessImmediately(int exit_code) {
  std::_Exit(exit_code);
}
#endif  // !STARBOARD

Process Process::Current() {
  return Process();
}

Process::Process(ProcessHandle handle) {}
Process::~Process() {}

void Process::TerminateCurrentProcessImmediately(int) {}

bool Process::IsValid() const { return false; }
bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const { return false; }
bool Process::Terminate(int exit_code, bool wait) const { return false; }
void Process::Close() {}

}  // namespace base
