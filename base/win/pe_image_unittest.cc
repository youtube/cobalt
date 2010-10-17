// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for PEImage.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/win/pe_image.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

// Just counts the number of invocations.
bool ExportsCallback(const PEImage &image,
                     DWORD ordinal,
                     DWORD hint,
                     LPCSTR name,
                     PVOID function,
                     LPCSTR forward,
                     PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool ImportsCallback(const PEImage &image,
                     LPCSTR module,
                     DWORD ordinal,
                     LPCSTR name,
                     DWORD hint,
                     PIMAGE_THUNK_DATA iat,
                     PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool SectionsCallback(const PEImage &image,
                       PIMAGE_SECTION_HEADER header,
                       PVOID section_start,
                       DWORD section_size,
                       PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool RelocsCallback(const PEImage &image,
                    WORD type,
                    PVOID address,
                    PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool ImportChunksCallback(const PEImage &image,
                          LPCSTR module,
                          PIMAGE_THUNK_DATA name_table,
                          PIMAGE_THUNK_DATA iat,
                          PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// Just counts the number of invocations.
bool DelayImportChunksCallback(const PEImage &image,
                               PImgDelayDescr delay_descriptor,
                               LPCSTR module,
                               PIMAGE_THUNK_DATA name_table,
                               PIMAGE_THUNK_DATA iat,
                               PIMAGE_THUNK_DATA bound_iat,
                               PIMAGE_THUNK_DATA unload_iat,
                               PVOID cookie) {
  int* count = reinterpret_cast<int*>(cookie);
  (*count)++;
  return true;
}

// We'll be using some known values for the tests.
enum Value {
  sections = 0,
  imports_dlls,
  delay_dlls,
  exports,
  imports,
  delay_imports,
  relocs
};

// Retrieves the expected value from advapi32.dll based on the OS.
int GetExpectedValue(Value value, DWORD os) {
  const int xp_delay_dlls = 2;
  const int xp_exports = 675;
  const int xp_imports = 422;
  const int xp_delay_imports = 8;
  const int xp_relocs = 9180;
  const int vista_delay_dlls = 4;
  const int vista_exports = 799;
  const int vista_imports = 476;
  const int vista_delay_imports = 24;
  const int vista_relocs = 10188;
  const int w2k_delay_dlls = 0;
  const int w2k_exports = 566;
  const int w2k_imports = 357;
  const int w2k_delay_imports = 0;
  const int w2k_relocs = 7388;
  const int win7_delay_dlls = 7;
  const int win7_exports = 806;
  const int win7_imports = 568;
  const int win7_delay_imports = 71;
  const int win7_relocs = 7812;

  // Contains the expected value, for each enumerated property (Value), and the
  // OS version: [Value][os_version]
  const int expected[][4] = {
    {4, 4, 4, 4},
    {3, 3, 3, 13},
    {w2k_delay_dlls, xp_delay_dlls, vista_delay_dlls, win7_delay_dlls},
    {w2k_exports, xp_exports, vista_exports, win7_exports},
    {w2k_imports, xp_imports, vista_imports, win7_imports},
    {w2k_delay_imports, xp_delay_imports,
     vista_delay_imports, win7_delay_imports},
    {w2k_relocs, xp_relocs, vista_relocs, win7_relocs}
  };

  if (value > relocs)
    return 0;
  if (50 == os)
    os = 0;  // 5.0
  else if (51 == os || 52 == os)
    os = 1;
  else if (os == 60)
    os = 2;  // 6.x
  else if (os >= 61)
    os = 3;
  else
    return 0;

  return expected[value][os];
}

// Tests that we are able to enumerate stuff from a PE file, and that
// the actual number of items found is within the expected range.
TEST(PEImageTest, EnumeratesPE) {
  // Windows Server 2003 is not supported as a test environment for this test.
  if (base::win::GetVersion() == base::win::VERSION_SERVER_2003)
    return;
  HMODULE module = LoadLibrary(L"advapi32.dll");
  ASSERT_TRUE(NULL != module);

  PEImage pe(module);
  int count = 0;
  EXPECT_TRUE(pe.VerifyMagic());

  DWORD os = pe.GetNTHeaders()->OptionalHeader.MajorOperatingSystemVersion;
  os = os * 10 + pe.GetNTHeaders()->OptionalHeader.MinorOperatingSystemVersion;

  pe.EnumSections(SectionsCallback, &count);
  EXPECT_EQ(GetExpectedValue(sections, os), count);

  count = 0;
  pe.EnumImportChunks(ImportChunksCallback, &count);
  EXPECT_EQ(GetExpectedValue(imports_dlls, os), count);

  count = 0;
  pe.EnumDelayImportChunks(DelayImportChunksCallback, &count);
  EXPECT_EQ(GetExpectedValue(delay_dlls, os), count);

  count = 0;
  pe.EnumExports(ExportsCallback, &count);
  EXPECT_GT(count, GetExpectedValue(exports, os) - 20);
  EXPECT_LT(count, GetExpectedValue(exports, os) + 100);

  count = 0;
  pe.EnumAllImports(ImportsCallback, &count);
  EXPECT_GT(count, GetExpectedValue(imports, os) - 20);
  EXPECT_LT(count, GetExpectedValue(imports, os) + 100);

  count = 0;
  pe.EnumAllDelayImports(ImportsCallback, &count);
  EXPECT_GT(count, GetExpectedValue(delay_imports, os) - 2);
  EXPECT_LT(count, GetExpectedValue(delay_imports, os) + 8);

  count = 0;
  pe.EnumRelocs(RelocsCallback, &count);
  EXPECT_GT(count, GetExpectedValue(relocs, os) - 150);
  EXPECT_LT(count, GetExpectedValue(relocs, os) + 1500);

  FreeLibrary(module);
}

// Tests that we can locate an specific exported symbol, by name and by ordinal.
TEST(PEImageTest, RetrievesExports) {
  HMODULE module = LoadLibrary(L"advapi32.dll");
  ASSERT_TRUE(NULL != module);

  PEImage pe(module);
  WORD ordinal;

  EXPECT_TRUE(pe.GetProcOrdinal("RegEnumKeyExW", &ordinal));

  FARPROC address1 = pe.GetProcAddress("RegEnumKeyExW");
  FARPROC address2 = pe.GetProcAddress(reinterpret_cast<char*>(ordinal));
  EXPECT_TRUE(address1 != NULL);
  EXPECT_TRUE(address2 != NULL);
  EXPECT_TRUE(address1 == address2);

  FreeLibrary(module);
}

}  // namespace win
}  // namespace base
