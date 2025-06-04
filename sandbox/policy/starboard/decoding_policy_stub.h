#ifndef SANDBOX_POLICY_LINUX_BPF_HARDWARE_VIDEO_DECODING_POLICY_LINUX_H_
#define SANDBOX_POLICY_LINUX_BPF_HARDWARE_VIDEO_DECODING_POLICY_LINUX_H_

#include "sandbox/policy/export.h"

namespace sandbox::policy {

// Policy used to sandbox utility processes that perform hardware video decoding
// on behalf of untrusted clients (Chrome renderer processes or ARC++/ARCVM).
//
// When making changes to this policy, ensure that you do not give access to
// privileged APIs (APIs that would allow these utility process to access data
// that's not explicitly shared with them through Mojo). For example, hardware
// video decoding processes should NEVER have access to /dev/dri/card* (the DRM
// master device).
class SANDBOX_POLICY_EXPORT HardwareVideoDecodingProcessPolicy {
 public:
  enum class PolicyType {
    kVaapiOnIntel,
    kVaapiOnAMD,
    kV4L2,
  };
  static PolicyType ComputePolicyType(bool use_amd_specific_policies);
};

}  // namespace sandbox::policy

#endif
