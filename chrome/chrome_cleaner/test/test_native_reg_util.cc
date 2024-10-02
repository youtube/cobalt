// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_native_reg_util.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/os/registry.h"

namespace chrome_cleaner_sandbox {

namespace {

const wchar_t kTimestampDelimiter[] = L"$";
const wchar_t kTempTestKeyPath[] =
    L"Software\\ChromeCleaner\\NativeTempTestKeys";

}  // namespace

ScopedTempRegistryKey::ScopedTempRegistryKey() {
  const base::TimeDelta timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch();
  key_path_ = base::StrCat(
      {kTempTestKeyPath, base::NumberToWString(timestamp.InMicroseconds()),
       kTimestampDelimiter,
       base::ASCIIToWide(base::Uuid::GenerateRandomV4().AsLowercaseString())});

  CHECK(ERROR_SUCCESS ==
        reg_key_.Create(HKEY_LOCAL_MACHINE, key_path_.c_str(), KEY_ALL_ACCESS));
  chrome_cleaner::GetNativeKeyPath(reg_key_, &fully_qualified_key_path_);
}

ScopedTempRegistryKey::~ScopedTempRegistryKey() {
  reg_key_.DeleteKey(L"");
}

HANDLE ScopedTempRegistryKey::Get() {
  return reg_key_.Handle();
}

const std::wstring& ScopedTempRegistryKey::Path() const {
  return key_path_;
}

const std::wstring& ScopedTempRegistryKey::FullyQualifiedPath() const {
  return fully_qualified_key_path_;
}

}  // namespace chrome_cleaner_sandbox
