// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_H__
#define BASE_FILE_VERSION_INFO_H__
#pragma once

#include <string>

#include "build/build_config.h"

class FilePath;

// Provides an interface for accessing the version information for a file.
// This is the information you access when you select a file in the Windows
// explorer, right-click select Properties, then click the Version tab.

class FileVersionInfo {
 public:
  virtual ~FileVersionInfo() {}
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Creates a FileVersionInfo for the specified path. Returns NULL if something
  // goes wrong (typically the file does not exit or cannot be opened). The
  // returned object should be deleted when you are done with it.
  static FileVersionInfo* CreateFileVersionInfo(const FilePath& file_path);
  // This version, taking a wstring, is deprecated and only kept around
  // until we can fix all callers.
  static FileVersionInfo* CreateFileVersionInfo(const std::wstring& file_path);
#endif

  // Creates a FileVersionInfo for the current module. Returns NULL in case
  // of error. The returned object should be deleted when you are done with it.
  static FileVersionInfo* CreateFileVersionInfoForCurrentModule();

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  virtual std::wstring company_name() = 0;
  virtual std::wstring company_short_name() = 0;
  virtual std::wstring product_name() = 0;
  virtual std::wstring product_short_name() = 0;
  virtual std::wstring internal_name() = 0;
  virtual std::wstring product_version() = 0;
  virtual std::wstring private_build() = 0;
  virtual std::wstring special_build() = 0;
  virtual std::wstring comments() = 0;
  virtual std::wstring original_filename() = 0;
  virtual std::wstring file_description() = 0;
  virtual std::wstring file_version() = 0;
  virtual std::wstring legal_copyright() = 0;
  virtual std::wstring legal_trademarks() = 0;
  virtual std::wstring last_change() = 0;
  virtual bool is_official_build() = 0;
};

#endif  // BASE_FILE_VERSION_INFO_H__
