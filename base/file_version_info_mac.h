// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_MAC_H_
#define BASE_FILE_VERSION_INFO_MAC_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/scoped_ptr.h"

#ifdef __OBJC__
@class NSBundle;
#else
class NSBundle;
#endif

class FilePath;

// Provides a way to access the version information for a file.
// This is the information you access when you select a file in the Windows
// explorer, right-click select Properties, then click the Version tab.

class FileVersionInfoMac : public FileVersionInfo {
 public:
  explicit FileVersionInfoMac(NSBundle *bundle);
  ~FileVersionInfoMac();

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

 private:
  // Lets you access other properties not covered above.
  bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  std::wstring GetStringValue(const wchar_t* name);

  NSBundle *bundle_;

  DISALLOW_COPY_AND_ASSIGN(FileVersionInfoMac);
};

#endif  // BASE_FILE_VERSION_INFO_MAC_H_
