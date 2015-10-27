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

// Starboard doesn't support the concept of processes, so this implementation
// just minimally and trivially implements the functions of this interface that
// are called, but that do not impact the functioning of the rest of the system.

#include "base/process.h"

#include "base/process_util.h"

namespace base {

// static
Process Process::Current() {
  return Process();
}

ProcessId Process::pid() const {
  return kNullProcessId;
}

bool Process::is_current() const {
  return true;
}

void Process::Close() {
  process_ = kNullProcessHandle;
}

void Process::Terminate(int result_code) {}

bool Process::IsProcessBackgrounded() const {
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  return false;
}

// static
bool Process::CanBackgroundProcesses() {
  return false;
}

int Process::GetPriority() const {
  return 0;
}

}  // namespace base
