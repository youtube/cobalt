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

#include <openssl/opensslconf.h>
#include <openssl/cpu.h>
#include "../../crypto/internal.h"
#include <starboard/cpu_features.h>
#include <starboard/string.h>

#if defined(STARBOARD)

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)

#define CPUID_01_REG_EDX        0
#define CPUID_01_REG_ECX        1
#define CPUID_07_REG_EBX        2
#define CPUID_07_REG_ECX        3

#define ID_01_EDX_IS_INTEL_BIT 30
#define ID_01_ECX_SSSE3_BIT     9
#define ID_01_ECX_AESNI_BIT    25
#define ID_01_ECX_AVX_BIT      28

static bool starboard_cpuid_setup_x86(void) {
    OPENSSL_ia32cap_P[CPUID_01_REG_EDX] = 0;
    OPENSSL_ia32cap_P[CPUID_01_REG_ECX] = 0;
    OPENSSL_ia32cap_P[CPUID_07_REG_EBX] = 0;
    OPENSSL_ia32cap_P[CPUID_07_REG_ECX] = 0;

    SbCPUFeatures features;
    if (!SbCPUFeaturesGet(&features)) {
        return false;
    }

    if(features.architecture != kSbCPUFeaturesArchitectureX86_64 &&
       features.architecture != kSbCPUFeaturesArchitectureX86) {
        return false;
    }

    // OpenSSL has optimized AVX codepath enabled only on Intel CPUs
    // The flag is passed as a "synthetic" value in reserved bit, i.e. CPUID
    // never returns it. See https://github.com/openssl/openssl/commit/0c14980
    if (!strcmp(features.x86.vendor,"GenuineIntel"))  {
        OPENSSL_ia32cap_P[CPUID_01_REG_EDX] |= (1 << ID_01_EDX_IS_INTEL_BIT);
    }

    // Enables AESNI acceleration
    if (features.x86.has_aesni)
        OPENSSL_ia32cap_P[CPUID_01_REG_ECX] |= (1 << ID_01_ECX_AESNI_BIT);
    // Enables AESNI and SHAx acceleration
    if (features.x86.has_avx)
        OPENSSL_ia32cap_P[CPUID_01_REG_ECX] |= (1 << ID_01_ECX_AVX_BIT);
    // Enables alternate SHA acceleration
    if (features.x86.has_ssse3)
        OPENSSL_ia32cap_P[CPUID_01_REG_ECX] |= (1 << ID_01_ECX_SSSE3_BIT);

    return true;
}

#else
static bool starboard_cpuid_setup_x86(void) { return true; }
#endif // defined(OPENSSL_X86 || OPENSSL_X86_64)

#if defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)

#include <openssl/arm_arch.h>

// This global provides ARM assembly routines capability flags
extern uint32_t OPENSSL_armcap_P;

static bool starboard_cpuid_setup_arm() {
    OPENSSL_armcap_P = 0; // Reset the flags

    SbCPUFeatures features;
    if (!SbCPUFeaturesGet(&features)) {
        return false;
    }

    if(features.architecture != kSbCPUFeaturesArchitectureArm64 &&
       features.architecture != kSbCPUFeaturesArchitectureArm) {
        return false;
    }

    if (features.arm.has_aes)
        OPENSSL_armcap_P |= ARMV8_AES;
    if (features.arm.has_sha1)
        OPENSSL_armcap_P |= ARMV8_SHA1;
    if (features.arm.has_sha2)
        OPENSSL_armcap_P |= ARMV8_SHA256;
    if (features.arm.has_pmull)
        OPENSSL_armcap_P |= ARMV8_PMULL;
    if (features.arm.has_neon)
        OPENSSL_armcap_P |= ARMV7_NEON;

    return true;
}

#else
static bool starboard_cpuid_setup_arm(void) { return true; }
#endif // OPENSSL_ARM || OPENSSL_AARCH64

void OPENSSL_cpuid_setup_starboard(void) {
    if (!starboard_cpuid_setup_arm() ||
        !starboard_cpuid_setup_x86() ) {
#if !SB_IS(EVERGREEN)
            // Fall back on original implementation if the platform
            // does not yet support Starboard CPU detection
            OPENSSL_cpuid_setup();
#endif
        }
}

#endif // STARBOARD
