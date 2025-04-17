// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/sandbox_linux_stub.h"

#include "base/notreached.h"

namespace sandbox {
namespace policy {

// Note: This is a stub implementation. The actual implementation is in
// sandbox_linux.cc.
// Stub symbols have been provided here to satisfy the linker and build cobalt

// static
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

void SandboxLinux::StartBrokerProcess(
    const syscall_broker::BrokerCommandSet& allowed_command_set,
    std::vector<syscall_broker::BrokerFilePermission> permissions,
    PreSandboxHook broker_side_hook,
    const Options& options) {
  NOTIMPLEMENTED();
}

SandboxLinux::SandboxLinux() {
  NOTIMPLEMENTED();
}

SandboxLinux::~SandboxLinux() {
  NOTIMPLEMENTED();
}

}  // namespace policy
}  // namespace sandbox
