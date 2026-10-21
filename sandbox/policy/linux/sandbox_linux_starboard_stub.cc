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

#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"  // nogncheck

#include "base/notreached.h"

namespace sandbox {
namespace policy {

// TODO: b/b/411720612 - Cobalt: Make sandbox changes which can
// be upstreamed to Chromium.
// Note: This is a stub implementation. The actual implementation is in
// sandbox_linux.cc which is not included in the cobalt build.
// Stub symbols have been provided here to satisfy the linker and build cobalt

SandboxLinux* SandboxLinux::GetInstance() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::vector<int> SandboxLinux::GetFileDescriptorsToClose() {
  NOTIMPLEMENTED();
  return std::vector<int>();
}

SetuidSandboxClient* SandboxLinux::setuid_sandbox_client() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void SandboxLinux::PreinitializeSandbox() {
  NOTIMPLEMENTED();
}

void SandboxLinux::EngageNamespaceSandbox(bool from_zygote) {
  NOTIMPLEMENTED();
}

bool SandboxLinux::EngageNamespaceSandboxIfPossible() {
  NOTIMPLEMENTED();
  return false;
}

bool SandboxLinux::InitializeSandbox(sandbox::mojom::Sandbox sandbox_type,
                                     PreSandboxHook hook,
                                     const Options& options) {
  NOTIMPLEMENTED();
  return false;
}

int SandboxLinux::GetStatus() {
  // Return 0, indicating no sandbox features are active.
  NOTIMPLEMENTED();
  return kInvalid;
}

SandboxLinux::SandboxLinux() {
  NOTIMPLEMENTED();
}

SandboxLinux::~SandboxLinux() {
  NOTIMPLEMENTED();
}

}  // namespace policy
}  // namespace sandbox
