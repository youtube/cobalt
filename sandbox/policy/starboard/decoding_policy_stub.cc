#include "sandbox/policy/starboard/decoding_policy_stub.h"

namespace sandbox::policy {

HardwareVideoDecodingProcessPolicy::PolicyType
HardwareVideoDecodingProcessPolicy::ComputePolicyType(bool) {
  return PolicyType::kVaapiOnIntel;
}

}  // namespace sandbox::policy
