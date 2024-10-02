// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_REGISTRY_TEST_DATA_H_
#define CHROME_INSTALLER_UTIL_REGISTRY_TEST_DATA_H_

#include <windows.h>

#include <string>

// A helper class for use by unit tests that need some registry space and data.
// BEWARE: Instances of this class irrevocably and recursively delete keys and
// values from the registry.  Carefully read the comments for Initialize and
// Reset before use.
class RegistryTestData {
 public:
  RegistryTestData();

  RegistryTestData(const RegistryTestData&) = delete;
  RegistryTestData& operator=(const RegistryTestData&) = delete;

  // Invokes Reset() on its way out.
  ~RegistryTestData();

  // Resets this instance, deletes the key rooted at |base_path|, and then
  // populates |base_path| with:
  // \EmptyKey
  // \NonEmptyKey (default value = "|base_path|\NonEmptyKey")
  // \NonEmptyKey\Subkey ("SomeValue" = DWORD 1)
  bool Initialize(HKEY root_key, const wchar_t* base_path);

  // Deletes the key rooted at base_path and clears all state.
  void Reset();

  // Fires Google Test expectations in the hopes that |path| contains the same
  // data as originally placed in |non_empty_key| by Initialize().
  void ExpectMatchesNonEmptyKey(HKEY root_key, const wchar_t* path);

  HKEY root_key() const { return root_key_; }
  const std::wstring& base_path() const { return base_path_; }
  const std::wstring& empty_key_path() const { return empty_key_path_; }
  const std::wstring& non_empty_key_path() const { return non_empty_key_path_; }

  // Fires Google Test expectations in the hopes that |path| is an empty key
  // (exists but has no values or subkeys).
  static void ExpectEmptyKey(HKEY root_key, const wchar_t* path);

 private:
  static bool DeleteKey(HKEY root_key, const wchar_t* path);

  HKEY root_key_;
  std::wstring base_path_;
  std::wstring empty_key_path_;
  std::wstring non_empty_key_path_;
};

#endif  // CHROME_INSTALLER_UTIL_REGISTRY_TEST_DATA_H_
