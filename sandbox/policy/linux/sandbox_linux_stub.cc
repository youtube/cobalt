// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/sandbox_linux_stub.h"

#include "base/notreached.h"

namespace sandbox {
namespace policy {

// Note: This is a stub implementation. The actual implementation is in
// sandbox_linux.cc, which is conditionally compiled.

// static
SandboxLinux* SandboxLinux::GetInstance() {
  // This stub is not expected to be used in a context where GetInstance() is
  // needed.
  NOTIMPLEMENTED();
  return nullptr;
}

std::vector<int> SandboxLinux::GetFileDescriptorsToClose() {
  // The stub implementation doesn't manage file descriptors.
  NOTIMPLEMENTED();
  return std::vector<int>();
}

SetuidSandboxClient* SandboxLinux::setuid_sandbox_client() const {
  // The stub implementation doesn't have a real setuid client.
  NOTIMPLEMENTED();
  return nullptr;
}

void SandboxLinux::PreinitializeSandbox() {
  // The stub implementation doesn't actually pre-initialize the sandbox.
  NOTIMPLEMENTED();
}

void SandboxLinux::EngageNamespaceSandbox(bool from_zygote) {
  // The stub implementation doesn't actually engage the namespace sandbox.
  NOTIMPLEMENTED();
}

bool SandboxLinux::EngageNamespaceSandboxIfPossible() {
  // The stub implementation doesn't actually engage the namespace sandbox.
  NOTIMPLEMENTED();
  return false;
}


bool SandboxLinux::InitializeSandbox(sandbox::mojom::Sandbox sandbox_type,
                                     PreSandboxHook hook,
                                     const Options& options) {
  // The stub implementation doesn't actually initialize a sandbox.
  NOTIMPLEMENTED();
  return false;
}

int SandboxLinux::GetStatus() {
  // Return 0, indicating no sandbox features are active.
  NOTIMPLEMENTED();
  return 0;
}

void SandboxLinux::StartBrokerProcess(
    const syscall_broker::BrokerCommandSet& allowed_command_set,
    std::vector<syscall_broker::BrokerFilePermission> permissions,
    PreSandboxHook broker_side_hook,
    const Options& options) {
  // The stub implementation doesn't start a broker process.
  NOTIMPLEMENTED();
}

SandboxLinux::SandboxLinux() {
  // Stub constructor.
  NOTIMPLEMENTED();
}

SandboxLinux::~SandboxLinux() {
  // Stub destructor.
  NOTIMPLEMENTED();
}

}  // namespace policy
}  // namespace sandbox


// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This could be in hardware_video_decoding_sandbox_hook_stub.cc
// or within #else/#endif in hardware_video_decoding_sandbox_hook_linux.cc

#include "media/gpu/sandbox/hardware_video_decoding_sandbox_hook_linux.h"

#include "base/logging.h" // Or base/notreached.h
#include "sandbox/policy/linux/sandbox_linux.h" // For SandboxLinux::Options

namespace media {

// This is a stub implementation for the hardware video decoding pre-sandbox
// hook. It allows the sandboxing process to continue but performs no actual
// setup.
// bool HardwareVideoDecodingPreSandboxHook(
//     sandbox::policy::SandboxLinux::Options options) {
//   VLOG(1) << "HardwareVideoDecodingPreSandboxHook: Stub implementation running.";
//   // Returning true allows the sandbox initialization to proceed.
//   // Returning false would typically abort the process startup.
//   return true;
// }

} // namespace media
