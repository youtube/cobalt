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

typedef struct SbCPUFeatures {
  // -----------------------------------------------------------------
  //     General processor information, valid on all architectures.
  // -----------------------------------------------------------------

  // Architecture of the processor.
  SbCPUFeaturesArchitecture architecture;

  // Processor brand string retrievable by CPUID or from /proc/cpuinfo
  // under the key "model name" or "Processor".
  const char* brand;

  // Processor cache line size in bytes. Queried from /proc/cpuinfo or
  // /proc/self/auxv, or CPUID with CLFLUSH instruction.
  uint32_t cache_size;

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
  uint16_t family;
  // Processor extended family ID, needs to be examined only when the
  // family ID is 0FH.
  uint16_t ext_family;
  // Processor model ID
  uint16_t model;
  // Processor extended model ID, needs to be examined only when the
  // family ID is 06H or 0FH.
  uint16_t ext_model;
  // Processor stepping ID, a product revision number
  uint16_t stepping;
  // Processor type ID
  uint16_t type;
  // A raw form of collection of processor stepping, model, and
  // family information
  uint16_t signature;

  // ---------------------------------------------------------------------
  //     Processor feature flags
  //
  //     These flags denote whether a feature is supported by the processor.
  //     They can be used easily to check one specific feature. The set of
  //     valid flags depends of specific architecture. Find details below.
  // ---------------------------------------------------------------------

  // ---------------------------------------------------------------------
  //     Processor feature flags valid on x86 and x86_64
  //
  //     See kernal source:
  //     https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/
  //     tree/arch/x86/include/asm/cpufeatures.h
  // ---------------------------------------------------------------------

  // ---------------------------------------------------------------------
  //     Intel-defined CPU features, CPUID level 0x00000001(EDX), word 0
  // ---------------------------------------------------------------------

  // Floating-point Unit on-chip.
  bool has_fpu;
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

  // ----------------------------------------------------------------------
  //     Processor feature flags valid only on Mips and Mips64
  //
  //     See MIPS SIMD documentation:
  //     https://www.mips.com/products/architectures/ase/simd/
  // ----------------------------------------------------------------------

  // MIPS SIMD Architecture (MSA).
  bool has_msa;

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
} SbCPUFeatures;

// Retrieve the underlying CPU features and place it in |features|, which must
// not be NULL.
//
// This function returns false if the CPU architecture is unknown. Otherwise,
// returns true.
SB_EXPORT bool SbCPUFeaturesGet(SbCPUFeatures* features);

#endif  // STARBOARD_CPU_FEATURES_H_
