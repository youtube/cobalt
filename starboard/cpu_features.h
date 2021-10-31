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

// Module Overview: Starboard CPU features API

// The Starboard API SbCPUFeaturesGet() queries the features of the underlying
// CPU.
//
// The features are returned in a structure SbCPUFeatures. It contains processor
// information like architecture, model and family, and capability flags that
// indicate whether certain features are supported, like NEON and VFP.
// The purpose of this API is to enable hardware optimizations if the
// architecture supports them.

#ifndef STARBOARD_CPU_FEATURES_H_
#define STARBOARD_CPU_FEATURES_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SbCPUFeaturesArchitecture {
  kSbCPUFeaturesArchitectureArm,
  kSbCPUFeaturesArchitectureArm64,
  kSbCPUFeaturesArchitectureMips,
  kSbCPUFeaturesArchitectureMips64,
  kSbCPUFeaturesArchitecturePpc,
  kSbCPUFeaturesArchitecturePpc64,
  kSbCPUFeaturesArchitectureX86,
  kSbCPUFeaturesArchitectureX86_64,
  kSbCPUFeaturesArchitectureUnknown,
} SbCPUFeaturesArchitecture;

typedef struct SbCPUFeaturesARM {
  // -------------------------------------------------------------------
  //     Processor version information, only valid on Arm and Arm64.
  //
  //     Stored in processor ID registers. See the reference manual of
  //     Cortex-M0 for instance: http://infocenter.arm.com/help/
  //     index.jsp?topic=/com.arm.doc.ddi0432c/Bhccjgga.html, and
  //     Cortex-A8 for instance: http://infocenter.arm.com/help/
  //     index.jsp?topic=/com.arm.doc.ddi0344k/Babififh.html.
  //     About how to retrieve the information, see Arm CPU Feature
  //     Registers documentation: https://www.kernel.org/doc/Documentation/
  //     arm64/cpu-feature-registers.txt, or retrieve from /proc/cpuinfo
  //     if available.
  // -------------------------------------------------------------------

  // Processor implementer/implementor code. ARM is 0x41, NVIDIA is 0x4e, etc.
  int16_t implementer;
  // Processor variant number, indicating the major revision number.
  int16_t variant;
  // Processor revision number, indicating the minor revision number.
  int16_t revision;
  // Processor architecture generation number, indicating the generations
  // (ARMv6-M, ARMv7, etc) within an architecture family. This field is
  // called "Architecture" or "Constant" in the processor ID register.
  int16_t architecture_generation;
  // Processor part number, indicating Cortex-M0, Cortex-A8, etc.
  int16_t part;

  // ------------------------------------------------------------------------
  //     Processor feature flags valid on Arm and Arm64
  //
  //     See ARM compiler armasm user guide:
  //     https://developer.arm.com/docs/100069/0604
  //     and Android NDK cpu features guide:
  //     https://developer.android.com/ndk/guides/cpu-features
  // ------------------------------------------------------------------------

  // ---------------------------------------------------------------------
  //     Arm 32 feature flags
  // ---------------------------------------------------------------------

  // ARM Advanced SIMD (NEON) vector instruction set extension.
  bool has_neon;
  // Thumb-2 mode.
  bool has_thumb2;
  // VFP (SIMD vector floating point instructions).
  bool has_vfp;
  // VFP version 3
  bool has_vfp3;
#if SB_API_VERSION >= 13
  // VFP version 4
  bool has_vfp4;
#endif
  // VFP version 3 with 32 D-registers.
  bool has_vfp3_d32;
  // SDIV and UDIV hardware division in ARM mode.
  bool has_idiva;

  // ---------------------------------------------------------------------
  //     Arm 64 feature flags
  // ---------------------------------------------------------------------
  // AES instructions.
  bool has_aes;
  // CRC32 instructions.
  bool has_crc32;
  // SHA-1 instructions.
  bool has_sha1;
  // SHA-256 instructions.
  bool has_sha2;
  // 64-bit PMULL and PMULL2 instructions.
  bool has_pmull;
} SbCPUFeaturesARM;

typedef struct SbCPUFeaturesMIPS {
  // ----------------------------------------------------------------------
  //     Processor feature flags valid only on Mips and Mips64
  //
  //     See MIPS SIMD documentation:
  //     https://www.mips.com/products/architectures/ase/simd/
  // ----------------------------------------------------------------------

  // MIPS SIMD Architecture (MSA).
  bool has_msa;
} SbCPUFeaturesMIPS;

typedef struct SbCPUFeaturesX86 {
  // -------------------------------------------------------------------
  //     Processor version information, valid only on x86 and x86_64
  //
  //     See Intel Advanced Vector Extensions Programming Reference:
  //     https://software.intel.com/file/36945.
  //     Version information is returned in EAX when CPUID executes with
  //     EAX = 0x00000001.
  // -------------------------------------------------------------------

  // Processor vendor ID string, e.g. "GenuineIntel", "AuthenticAMD", etc
  const char* vendor;
  // Processor family ID
  int16_t family;
  // Processor extended family ID, needs to be examined only when the
  // family ID is 0FH.
  int16_t ext_family;
  // Processor model ID
  int16_t model;
  // Processor extended model ID, needs to be examined only when the
  // family ID is 06H or 0FH.
  int16_t ext_model;
  // Processor stepping ID, a product revision number
  int16_t stepping;
  // Processor type ID
  int16_t type;
  // A raw form of collection of processor stepping, model, and
  // family information
  int32_t signature;

  // ---------------------------------------------------------------------
  //     Processor feature flags
  //
  //     These flags denote whether a feature is supported by the processor.
  //     They can be used easily to check one specific feature. The set of
  //     valid flags depends of specific architecture. Below are processor
  //     feature flags valid on x86 and x86_64
  //
  //     See kernel source:
  //     https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/
  //     tree/arch/x86/include/asm/cpufeatures.h
  // ---------------------------------------------------------------------

  // ---------------------------------------------------------------------
  //     Intel-defined CPU features, CPUID level 0x00000001(EDX), word 0
  // ---------------------------------------------------------------------

  // Conditional Move Instructions (plus FCMOVcc, FCOMI with FPU).
  bool has_cmov;
  // Multimedia extensions.
  bool has_mmx;
  // SSE (Streaming SIMD Extensions).
  bool has_sse;
  // SSE2 extensions.
  bool has_sse2;
  // Time Stamp Counter.
  bool has_tsc;

  // ---------------------------------------------------------------------
  //     Intel-defined CPU features, CPUID level 0x00000001(ECX), word 4
  // ---------------------------------------------------------------------

  // SSE3 extensions.
  bool has_sse3;
#if SB_API_VERSION >= 12
  // PCLMULQDQ instruction.
  bool has_pclmulqdq;
#endif  // SB_API_VERSION >= 12
  // Supplemental SSE3 extensions.
  bool has_ssse3;
  // SSE-4.1 extensions.
  bool has_sse41;
  // SSE-4.2 extensions.
  bool has_sse42;
  // MOVBE instruction.
  bool has_movbe;
  // POPCNT instruction.
  bool has_popcnt;
  // XSAVE/XRSTOR/XGETBV/XSETBV instruction enabled in this OS.
  bool has_osxsave;
  // AVX (Advanced Vector Extensions).
  bool has_avx;
  // 16-bit FP conversions.
  bool has_f16c;
  // Fused multiply-add.
  bool has_fma3;
  // AES new instructions (AES-NI).
  bool has_aesni;

  // ---------------------------------------------------------------------
  //     Intel-defined CPU features, CPUID level 0x00000007:0(EBX), word 9
  // ---------------------------------------------------------------------

  // AVX2 Extensions.
  bool has_avx2;
  // AVX-512 Foundation.
  bool has_avx512f;
  // AVX-512 DQ (Double/Quad granular) Instructions.
  bool has_avx512dq;
  // AVX-512 Integer Fused Multiply-Add instructions.
  bool has_avx512ifma;
  // AVX-512 Prefetch.
  bool has_avx512pf;
  // AVX-512 Exponential and Reciprocal.
  bool has_avx512er;
  // AVX-512 Conflict Detection.
  bool has_avx512cd;
  // AVX-512 BW (Byte/Word granular) Instructions.
  bool has_avx512bw;
  // AVX-512 VL (128/256 Vector Length) Extensions.
  bool has_avx512vl;
  // First group of bit manipulation extensions.
  bool has_bmi1;
  // Second group of bit manipulation extensions.
  bool has_bmi2;

  // ---------------------------------------------------------------------
  //     Intel-defined CPU features, CPUID level 0x80000001:0(ECX)
  // ---------------------------------------------------------------------

  // Bit manipulation instruction LZCNT.
  bool has_lzcnt;

  // ---------------------------------------------------------------------
  //     AMD-defined CPU features: CPUID level 0x80000001(ECX), word 6
  // ---------------------------------------------------------------------

  // SAHF in long mode.
  bool has_sahf;
} SbCPUFeaturesX86;

typedef struct SbCPUFeatures {
  // -----------------------------------------------------------------
  //     General processor information, valid on all architectures.
  // -----------------------------------------------------------------

  // Architecture of the processor.
  SbCPUFeaturesArchitecture architecture;

  // Processor brand string retrievable by CPUID or from /proc/cpuinfo
  // under the key "model name" or "Processor".
  const char* brand;

  // Processor cache line size in bytes of Level 1 instruction cache and data
  // cache. Queried by sysconf(_SC_LEVEL1_ICACHE_LINESIZE) and
  // sysconf(_SC_LEVEL1_DCACHE_LINESIZE), or from files /proc/cpuinfo,
  // /proc/self/auxv, or CPUID with CLFLUSH instruction.
  int32_t icache_line_size;
  int32_t dcache_line_size;

  // Processor has floating-point unit on-chip.
  bool has_fpu;

  // ------------------------------------------------------------------
  //     Processor feature flags bitmask, valid on the architectures where
  //     libc’s getauxval() is supported.
  //
  //     These are compact bitmasks containing information of the feature
  //     flags. The bitmasks hold different flags depending on specific
  //     architectures. See third_party/musl/arch/<arch_name>/bits/hwcap.h
  //     for details. The flags contained by the bitmasks may be broader
  //     than the features flags contained by the SbCPUFeatures. The
  //     recommendation of usage is to never use these bitmasks as long as
  //     the information is available as a field of SbCPUFeatures, and
  //     only rely on bitmasks if the information is not natively supported
  //     by SbCPUFeatures.
  // ------------------------------------------------------------------

  // Bit-mask containing processor features flags. Queried by
  // getauxval(AT_HWCAP) if it is supported.
  uint32_t hwcap;
  // Similar to hwcap. Queried by getauxval(AT_HWCAP2) if
  // it is supported.
  uint32_t hwcap2;

  // Processor features specific to each architecture. Set the appropriate
  // |SbCPUFeatures<ARCH_NAME>| for the underlying architecture. Set the
  // other |SbCPUFeatures<OTHER_ARCH>| to be invalid, meaning:
  // - '-1' for signed integer fields
  // - 'false' for feature flag fields
  // - '0' for feature flag bitmasks
  // - empty string for string fields
  SbCPUFeaturesARM arm;
  // The reason that the "_arch" suffix exists for mips is because on some
  // platforms that use MIPS as the underlying architecture, "mips" is
  // already defined as a macro.
  SbCPUFeaturesMIPS mips_arch;
  SbCPUFeaturesX86 x86;
} SbCPUFeatures;

// Retrieve the underlying CPU features and place it in |features|, which must
// not be NULL.
//
// If this function returns false, it means the CPU architecture is unknown and
// all fields in |features| are invalid.
SB_EXPORT bool SbCPUFeaturesGet(SbCPUFeatures* features);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_CPU_FEATURES_H_
