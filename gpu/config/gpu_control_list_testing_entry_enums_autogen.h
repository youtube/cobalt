// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ENTRY_ENUMS_AUTOGEN_H_
#define GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ENTRY_ENUMS_AUTOGEN_H_

namespace gpu {
enum GpuControlListTestingEntryEnum {
  kGpuControlListEntryTest_DetailedEntry = 0,
  kGpuControlListEntryTest_VendorOnAllOsEntry = 1,
  kGpuControlListEntryTest_VendorOnLinuxEntry = 2,
  kGpuControlListEntryTest_AllExceptNVidiaOnLinuxEntry = 3,
  kGpuControlListEntryTest_AllExceptIntelOnLinuxEntry = 4,
  kGpuControlListEntryTest_MultipleDevicesEntry = 5,
  kGpuControlListEntryTest_ChromeOSEntry = 6,
  kGpuControlListEntryTest_GlVersionGLESEntry = 7,
  kGpuControlListEntryTest_GlVersionANGLEEntry = 8,
  kGpuControlListEntryTest_GlVersionGLEntry = 9,
  kGpuControlListEntryTest_GlVendorEqual = 10,
  kGpuControlListEntryTest_GlVendorWithDot = 11,
  kGpuControlListEntryTest_GlRendererContains = 12,
  kGpuControlListEntryTest_GlRendererCaseInsensitive = 13,
  kGpuControlListEntryTest_GlExtensionsEndWith = 14,
  kGpuControlListEntryTest_OptimusEntry = 15,
  kGpuControlListEntryTest_AMDSwitchableEntry = 16,
  kGpuControlListEntryTest_DriverVendorBeginWith = 17,
  kGpuControlListEntryTest_LexicalDriverVersionEntry = 18,
  kGpuControlListEntryTest_NeedsMoreInfoEntry = 19,
  kGpuControlListEntryTest_NeedsMoreInfoForExceptionsEntry = 20,
  kGpuControlListEntryTest_NeedsMoreInfoForGlVersionEntry = 21,
  kGpuControlListEntryTest_FeatureTypeAllEntry = 22,
  kGpuControlListEntryTest_FeatureTypeAllEntryWithExceptions = 23,
  kGpuControlListEntryTest_SingleActiveGPU = 24,
  kGpuControlListEntryTest_MachineModelName = 25,
  kGpuControlListEntryTest_MachineModelNameException = 26,
  kGpuControlListEntryTest_MachineModelVersion = 27,
  kGpuControlListEntryTest_MachineModelVersionException = 28,
  kGpuControlListEntryDualGPUTest_CategoryAny_Intel = 29,
  kGpuControlListEntryDualGPUTest_CategoryAny_NVidia = 30,
  kGpuControlListEntryDualGPUTest_CategorySecondary = 31,
  kGpuControlListEntryDualGPUTest_CategoryPrimary = 32,
  kGpuControlListEntryDualGPUTest_CategoryDefault = 33,
  kGpuControlListEntryDualGPUTest_ActiveSecondaryGPU = 34,
  kGpuControlListEntryDualGPUTest_VendorOnlyActiveSecondaryGPU = 35,
  kGpuControlListEntryDualGPUTest_ActivePrimaryGPU = 36,
  kGpuControlListEntryDualGPUTest_VendorOnlyActivePrimaryGPU = 37,
  kGpuControlListEntryTest_PixelShaderVersion = 38,
  kGpuControlListEntryTest_OsVersionZeroLT = 39,
  kGpuControlListEntryTest_OsVersionZeroAny = 40,
  kGpuControlListEntryTest_OsComparisonAny = 41,
  kGpuControlListEntryTest_OsComparisonGE = 42,
  kGpuControlListEntryTest_ExceptionWithoutVendorId = 43,
  kGpuControlListEntryTest_MultiGpuStyleAMDSwitchableDiscrete = 44,
  kGpuControlListEntryTest_MultiGpuStyleAMDSwitchableIntegrated = 45,
  kGpuControlListEntryTest_InProcessGPU = 46,
  kGpuControlListEntryTest_SameGPUTwiceTest = 47,
  kGpuControlListEntryTest_NVidiaNumberingScheme = 48,
  kGpuControlListTest_NeedsMoreInfo = 49,
  kGpuControlListTest_NeedsMoreInfoForExceptions = 50,
  kGpuControlListTest_IgnorableEntries_0 = 51,
  kGpuControlListTest_IgnorableEntries_1 = 52,
  kGpuControlListTest_DisabledExtensionTest_0 = 53,
  kGpuControlListTest_DisabledExtensionTest_1 = 54,
  kGpuControlListEntryTest_DirectRendering = 55,
  kGpuControlListTest_LinuxKernelVersion = 56,
  kGpuControlListTest_TestGroup_0 = 57,
  kGpuControlListTest_TestGroup_1 = 58,
  kGpuControlListEntryTest_GpuSeries = 59,
  kGpuControlListEntryTest_GpuSeriesActive = 60,
  kGpuControlListEntryTest_GpuSeriesAny = 61,
  kGpuControlListEntryTest_GpuSeriesPrimary = 62,
  kGpuControlListEntryTest_GpuSeriesSecondary = 63,
  kGpuControlListEntryTest_GpuSeriesInException = 64,
  kGpuControlListEntryTest_MultipleDrivers = 65,
  kGpuControlListEntryTest_HardwareOverlay = 66,
  kGpuControlListEntryTest_GpuGeneration = 67,
  kGpuControlListEntryTest_GpuGenerationActive = 68,
  kGpuControlListEntryTest_GpuGenerationAny = 69,
  kGpuControlListEntryTest_GpuGenerationPrimary = 70,
  kGpuControlListEntryTest_GpuGenerationSecondary = 71,
  kGpuControlListEntryTest_SubpixelFontRendering = 72,
  kGpuControlListEntryTest_SubpixelFontRenderingDontCare = 73,
  kGpuControlListEntryTest_IntelDriverVendorEntry = 74,
  kGpuControlListEntryTest_IntelDriverVersionEntry = 75,
  kGpuControlListEntryTest_DeviceRevisionEntry = 76,
  kGpuControlListEntryTest_DeviceRevisionUnspecifiedEntry = 77,
  kGpuControlListEntryTest_AnyDriverVersion = 78,
  kGpuControlListEntryTest_ActiveDriverVersion = 79,
};
}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ENTRY_ENUMS_AUTOGEN_H_
