// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/seccomp_support_detector.h"

#include <stdio.h>
#include <sys/utsname.h>

#include "base/cpu.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace {

// Reports the kernel version obtained from uname.
void ReportKernelVersion() {
  // This method will report the kernel major and minor versions by
  // taking the lower 16 bits of each version number and combining
  // the two into a 32-bit number.

  utsname uts;
  if (uname(&uts) == 0) {
    int major, minor;
    if (sscanf(uts.release, "%d.%d", &major, &minor) == 2) {
      int version = ((major & 0xFFFF) << 16) | (minor & 0xFFFF);
      base::UmaHistogramSparse("Android.KernelVersion", version);
    }
  }
}

// Per the comment in base/cpu.cc's ParseProcCpu(), unfortunately there is not
// a universally reliable way to examine the CPU part information for all
// cores, so the sampling effect of data collection via UMA will have to
// suffice.
void ReportArmCpu() {
#if defined(ARCH_CPU_ARM_FAMILY)
  base::CPU cpu;

  // Compose the implementer (8 bits) and the part number (12 bits) into a
  // single 20-bit number that can be recorded via UMA.
  uint32_t composed = cpu.implementer();
  composed <<= 12;
  composed |= cpu.part_number();
  base::UmaHistogramSparse("Android.ArmCpuPart", composed);
#endif
}

}  // namespace

void ReportSeccompSupport() {
  ReportKernelVersion();
  ReportArmCpu();
}
