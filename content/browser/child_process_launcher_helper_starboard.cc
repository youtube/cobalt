// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <optional>

#include "base/notreached.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"

namespace content {
namespace internal {

std::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  NOTIMPLEMENTED();
}

std::unique_ptr<PosixFileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  NOTIMPLEMENTED();
  return false;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  ChildProcessLauncherHelper::Process process;
  NOTIMPLEMENTED();
  return process;
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  NOTIMPLEMENTED();
  return false;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  NOTIMPLEMENTED();
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  NOTIMPLEMENTED();
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  NOTIMPLEMENTED();
  return false;
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  NOTIMPLEMENTED();
}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  NOTREACHED();
}

}  // namespace internal
}  // namespace content
