---
layout: doc
title: "Starboard Module Reference: cpu_features.h"
---

## Structs ##

### SbCPUFeatures ###

#### Members ####

*   `SbCPUFeaturesArchitecture architecture`

    Architecture of the processor.
*   `const char * brand`

    Processor brand string retrievable by CPUID or from /proc/cpuinfo under the
    key "model name" or "Processor".
*   `int32_t icache_line_size`

    Processor cache line size in bytes of Level 1 instruction cache and data
    cache. Queried by sysconf(_SC_LEVEL1_ICACHE_LINESIZE) and
    sysconf(_SC_LEVEL1_DCACHE_LINESIZE), or from files /proc/cpuinfo,
    /proc/self/auxv, or CPUID with CLFLUSH instruction.
*   `int32_t dcache_line_size`
*   `bool has_fpu`

    Processor has floating-point unit on-chip.
*   `uint32_t hwcap`

    Bit-mask containing processor features flags. Queried by getauxval(AT_HWCAP)
    if it is supported.
*   `uint32_t hwcap2`

    Similar to hwcap. Queried by getauxval(AT_HWCAP2) if it is supported.
*   `SbCPUFeaturesARM arm`

    Processor features specific to each architecture. Set the appropriate
    `SbCPUFeatures<ARCH_NAME>` for the underlying architecture. Set the other
    `SbCPUFeatures<OTHER_ARCH>` to be invalid, meaning:

    *   '-1' for signed integer fields

    *   'false' for feature flag fields

    *   '0' for feature flag bitmasks

    *   empty string for string fields

*   `SbCPUFeaturesMIPS mips_arch`

    The reason that the "_arch" suffix exists for mips is because on some
    platforms that use MIPS as the underlying architecture, "mips" is already
    defined as a macro.
*   `SbCPUFeaturesX86 x86`

### SbCPUFeaturesARM ###

#### Members ####

*   `int16_t implementer`

    Processor implementer/implementor code. ARM is 0x41, NVIDIA is 0x4e, etc.
*   `int16_t variant`

    Processor variant number, indicating the major revision number.
*   `int16_t revision`

    Processor revision number, indicating the minor revision number.
*   `int16_t architecture_generation`

    Processor architecture generation number, indicating the generations
    (ARMv6-M, ARMv7, etc) within an architecture family. This field is called
    "Architecture" or "Constant" in the processor ID register.
*   `int16_t part`

    Processor part number, indicating Cortex-M0, Cortex-A8, etc.
*   `bool has_neon`

    ARM Advanced SIMD (NEON) vector instruction set extension.
*   `bool has_thumb2`

    Thumb-2 mode.
*   `bool has_vfp`

    VFP (SIMD vector floating point instructions).
*   `bool has_vfp3`

    VFP version 3
*   `bool has_vfp4`

    VFP version 4
*   `bool has_vfp3_d32`

    VFP version 3 with 32 D-registers.
*   `bool has_idiva`

    SDIV and UDIV hardware division in ARM mode.
*   `bool has_aes`

    ###### Arm 64 feature flags  ######

    AES instructions.
*   `bool has_crc32`

    CRC32 instructions.
*   `bool has_sha1`

    SHA-1 instructions.
*   `bool has_sha2`

    SHA-256 instructions.
*   `bool has_pmull`

    64-bit PMULL and PMULL2 instructions.

### SbCPUFeaturesMIPS ###

#### Members ####

*   `bool has_msa`

    MIPS SIMD Architecture (MSA).

### SbCPUFeaturesX86 ###

#### Members ####

*   `const char * vendor`

    Processor vendor ID string, e.g. "GenuineIntel", "AuthenticAMD", etc
*   `int16_t family`

    Processor family ID
*   `int16_t ext_family`

    Processor extended family ID, needs to be examined only when the family ID
    is 0FH.
*   `int16_t model`

    Processor model ID
*   `int16_t ext_model`

    Processor extended model ID, needs to be examined only when the family ID is
    06H or 0FH.
*   `int16_t stepping`

    Processor stepping ID, a product revision number
*   `int16_t type`

    Processor type ID
*   `int32_t signature`

    A raw form of collection of processor stepping, model, and family
    information
*   `bool has_cmov`

    Conditional Move Instructions (plus FCMOVcc, FCOMI with FPU).
*   `bool has_mmx`

    Multimedia extensions.
*   `bool has_sse`

    SSE (Streaming SIMD Extensions).
*   `bool has_sse2`

    SSE2 extensions.
*   `bool has_tsc`

    Time Stamp Counter.
*   `bool has_sse3`

    SSE3 extensions.
*   `bool has_pclmulqdq`

    PCLMULQDQ instruction.
*   `bool has_ssse3`

    Supplemental SSE3 extensions.
*   `bool has_sse41`

    SSE-4.1 extensions.
*   `bool has_sse42`

    SSE-4.2 extensions.
*   `bool has_movbe`

    MOVBE instruction.
*   `bool has_popcnt`

    POPCNT instruction.
*   `bool has_osxsave`

    XSAVE/XRSTOR/XGETBV/XSETBV instruction enabled in this OS.
*   `bool has_avx`

    AVX (Advanced Vector Extensions).
*   `bool has_f16c`

    16-bit FP conversions.
*   `bool has_fma3`

    Fused multiply-add.
*   `bool has_aesni`

    AES new instructions (AES-NI).
*   `bool has_avx2`

    AVX2 Extensions.
*   `bool has_avx512f`

    AVX-512 Foundation.
*   `bool has_avx512dq`

    AVX-512 DQ (Double/Quad granular) Instructions.
*   `bool has_avx512ifma`

    AVX-512 Integer Fused Multiply-Add instructions.
*   `bool has_avx512pf`

    AVX-512 Prefetch.
*   `bool has_avx512er`

    AVX-512 Exponential and Reciprocal.
*   `bool has_avx512cd`

    AVX-512 Conflict Detection.
*   `bool has_avx512bw`

    AVX-512 BW (Byte/Word granular) Instructions.
*   `bool has_avx512vl`

    AVX-512 VL (128/256 Vector Length) Extensions.
*   `bool has_bmi1`

    First group of bit manipulation extensions.
*   `bool has_bmi2`

    Second group of bit manipulation extensions.
*   `bool has_lzcnt`

    Bit manipulation instruction LZCNT.
*   `bool has_sahf`

    SAHF in long mode.

## Functions ##

### SbCPUFeaturesGet ###

Retrieve the underlying CPU features and place it in `features`, which must not
be NULL.

If this function returns false, it means the CPU architecture is unknown and all
fields in `features` are invalid.

#### Declaration ####

```
bool SbCPUFeaturesGet(SbCPUFeatures *features)
```
