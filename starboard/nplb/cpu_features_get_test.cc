// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <cstring>

#include "starboard/cpu_features.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

constexpr int kFeatureValueInvalid = -1;

void ExpectArmInvalid(const SbCPUFeatures& features) {
  EXPECT_EQ(kFeatureValueInvalid, features.arm.implementer);
  EXPECT_EQ(kFeatureValueInvalid, features.arm.variant);
  EXPECT_EQ(kFeatureValueInvalid, features.arm.revision);
  EXPECT_EQ(kFeatureValueInvalid, features.arm.architecture_generation);
  EXPECT_EQ(kFeatureValueInvalid, features.arm.part);

  EXPECT_EQ(false, features.arm.has_neon);
  EXPECT_EQ(false, features.arm.has_thumb2);
  EXPECT_EQ(false, features.arm.has_vfp);
  EXPECT_EQ(false, features.arm.has_vfp3);
#if SB_API_VERSION >= 13
  EXPECT_EQ(false, features.arm.has_vfp4);
#endif
  EXPECT_EQ(false, features.arm.has_vfp3_d32);
  EXPECT_EQ(false, features.arm.has_idiva);
  EXPECT_EQ(false, features.arm.has_aes);
  EXPECT_EQ(false, features.arm.has_crc32);
  EXPECT_EQ(false, features.arm.has_sha1);
  EXPECT_EQ(false, features.arm.has_sha2);
  EXPECT_EQ(false, features.arm.has_pmull);
}

void ExpectMipsInvalid(const SbCPUFeatures& features) {
  EXPECT_EQ(false, features.mips_arch.has_msa);
}

void ExpectX86Invalid(const SbCPUFeatures& features) {
  EXPECT_NE(nullptr, features.x86.vendor);
  EXPECT_EQ(0, strlen(features.x86.vendor));
  EXPECT_EQ(kFeatureValueInvalid, features.x86.family);
  EXPECT_EQ(kFeatureValueInvalid, features.x86.ext_family);
  EXPECT_EQ(kFeatureValueInvalid, features.x86.model);
  EXPECT_EQ(kFeatureValueInvalid, features.x86.ext_model);
  EXPECT_EQ(kFeatureValueInvalid, features.x86.stepping);
  EXPECT_EQ(kFeatureValueInvalid, features.x86.type);
  EXPECT_EQ(kFeatureValueInvalid, features.x86.signature);

  EXPECT_EQ(false, features.x86.has_cmov);
  EXPECT_EQ(false, features.x86.has_mmx);
  EXPECT_EQ(false, features.x86.has_sse);
  EXPECT_EQ(false, features.x86.has_sse2);
  EXPECT_EQ(false, features.x86.has_tsc);
  EXPECT_EQ(false, features.x86.has_sse3);
#if SB_API_VERSION >= 12
  EXPECT_EQ(false, features.x86.has_pclmulqdq);
#endif  // SB_API_VERSION >= 12
  EXPECT_EQ(false, features.x86.has_ssse3);
  EXPECT_EQ(false, features.x86.has_sse41);
  EXPECT_EQ(false, features.x86.has_sse42);
  EXPECT_EQ(false, features.x86.has_movbe);
  EXPECT_EQ(false, features.x86.has_popcnt);
  EXPECT_EQ(false, features.x86.has_osxsave);
  EXPECT_EQ(false, features.x86.has_avx);
  EXPECT_EQ(false, features.x86.has_f16c);
  EXPECT_EQ(false, features.x86.has_fma3);
  EXPECT_EQ(false, features.x86.has_aesni);
  EXPECT_EQ(false, features.x86.has_avx2);
  EXPECT_EQ(false, features.x86.has_avx512f);
  EXPECT_EQ(false, features.x86.has_avx512dq);
  EXPECT_EQ(false, features.x86.has_avx512ifma);
  EXPECT_EQ(false, features.x86.has_avx512pf);
  EXPECT_EQ(false, features.x86.has_avx512er);
  EXPECT_EQ(false, features.x86.has_avx512cd);
  EXPECT_EQ(false, features.x86.has_avx512bw);
  EXPECT_EQ(false, features.x86.has_avx512vl);
  EXPECT_EQ(false, features.x86.has_bmi1);
  EXPECT_EQ(false, features.x86.has_bmi2);
  EXPECT_EQ(false, features.x86.has_lzcnt);
  EXPECT_EQ(false, features.x86.has_sahf);
}

TEST(SbCPUFeaturesGetTest, SunnyDay) {
  SbCPUFeatures features;

  bool result = SbCPUFeaturesGet(&features);
  if (!result) {
    EXPECT_EQ(kSbCPUFeaturesArchitectureUnknown, features.architecture);
    EXPECT_NE(nullptr, features.brand);
    EXPECT_EQ(0, strlen(features.brand));
    EXPECT_EQ(kFeatureValueInvalid, features.icache_line_size);
    EXPECT_EQ(kFeatureValueInvalid, features.dcache_line_size);
    EXPECT_FALSE(features.has_fpu);
    EXPECT_EQ(0, features.hwcap);
    EXPECT_EQ(0, features.hwcap2);
    ExpectArmInvalid(features);
    ExpectMipsInvalid(features);
    ExpectX86Invalid(features);
    return;
  } else {
    // If the architecture is unknown, SbCPUFeaturesGet() must return
    // false.
    EXPECT_NE(kSbCPUFeaturesArchitectureUnknown, features.architecture);

#if SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)
    EXPECT_TRUE(features.architecture == kSbCPUFeaturesArchitectureArm ||
                features.architecture == kSbCPUFeaturesArchitectureArm64);
#else  // !SB_IS(ARCH_ARM) && !SB_IS(ARCH_ARM64)
    ExpectArmInvalid(features);
#endif  // SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)

#if SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
    EXPECT_TRUE(features.architecture == kSbCPUFeaturesArchitectureX86 ||
                features.architecture == kSbCPUFeaturesArchitectureX86_64);
#else   // !SB_IS(ARCH_X86) && !SB_IS(ARCH_X64)
    ExpectX86Invalid(features);
#endif  // SB_IS(ARCH_X86) || SB_IS(ARCH_X64)

    ExpectMipsInvalid(features);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
