// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_WIN_H_
#define BASE_FILE_VERSION_INFO_WIN_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/scoped_ptr.h"

struct tagVS_FIXEDFILEINFO;
typedef tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;

// Provides a way to access the version information for a file.
// This is the information you access when you select a file in the Windows
// explorer, right-click select Properties, then click the Version tab.

class FileVersionInfoWin : public FileVersionInfo {
 public:
  FileVersionInfoWin(void* data, int language, int code_page);
  ~FileVersionInfoWin();

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  virtual std::wstring company_name();
  virtual std::wstring company_short_name();
  virtual std::wstring product_name();
  virtual std::wstring product_short_name();
  virtual std::wstring internal_name();
  virtual std::wstring product_version();
  virtual std::wstring private_build();
  virtual std::wstring special_build();
  virtual std::wstring comments();
  virtual std::wstring original_filename();
  virtual std::wstring file_description();
  virtual std::wstring file_version();
  virtual std::wstring legal_copyright();
  virtual std::wstring legal_trademarks();
  virtual std::wstring last_change();
  virtual bool is_official_build();

  // Lets you access other properties not covered above.
  bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  std::wstring GetStringValue(const wchar_t* name);

  // Get the fixed file info if it exists. Otherwise NULL
  VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }

 private:
  scoped_ptr_malloc<char> data_;
  int language_;
  int code_page_;
  // This is a pointer into the data_ if it exists. Otherwise NULL.
  VS_FIXEDFILEINFO* fixed_file_info_;

  DISALLOW_COPY_AND_ASSIGN(FileVersionInfoWin);
};

#endif  // BASE_FILE_VERSION_INFO_WIN_H_
