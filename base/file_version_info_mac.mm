// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

FileVersionInfo::FileVersionInfo(NSBundle *bundle) : bundle_(bundle) {
  [bundle_ retain];
}

FileVersionInfo::~FileVersionInfo() {
  [bundle_ release];
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  // TODO(erikkay): this should really use bundleForClass, but we don't have
  // a class to hang onto yet.
  NSBundle* bundle = [NSBundle mainBundle];
  return new FileVersionInfo(bundle);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const std::wstring& file_path) {
  NSString* path = [NSString stringWithCString:
      reinterpret_cast<const char*>(file_path.c_str())
        encoding:NSUTF32StringEncoding];
  return new FileVersionInfo([NSBundle bundleWithPath:path]);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const FilePath& file_path) {
  NSString* path = [NSString stringWithUTF8String:file_path.value().c_str()];
  return new FileVersionInfo([NSBundle bundleWithPath:path]);
}

std::wstring FileVersionInfo::company_name() {
  return std::wstring();
}

std::wstring FileVersionInfo::company_short_name() {
  return std::wstring();
}

std::wstring FileVersionInfo::internal_name() {
  return std::wstring();
}

std::wstring FileVersionInfo::product_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::product_short_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::comments() {
  return std::wstring();
}

std::wstring FileVersionInfo::legal_copyright() {
  return GetStringValue(L"CFBundleGetInfoString");
}

std::wstring FileVersionInfo::product_version() {
  return GetStringValue(L"CFBundleShortVersionString");
}

std::wstring FileVersionInfo::file_description() {
  return std::wstring();
}

std::wstring FileVersionInfo::legal_trademarks() {
  return std::wstring();
}

std::wstring FileVersionInfo::private_build() {
  return std::wstring();
}

std::wstring FileVersionInfo::file_version() {
  return product_version();
}

std::wstring FileVersionInfo::original_filename() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::special_build() {
  return std::wstring();
}

std::wstring FileVersionInfo::last_change() {
  return GetStringValue(L"SVNRevision");
}

bool FileVersionInfo::is_official_build() {
#if defined (GOOGLE_CHROME_BUILD)
  return true;
#else
  return false;
#endif
}

bool FileVersionInfo::GetValue(const wchar_t* name, std::wstring* value_str) {
  if (bundle_) {
    NSString* value = [bundle_ objectForInfoDictionaryKey:
        [NSString stringWithUTF8String:WideToUTF8(name).c_str()]];
    if (value) {
      *value_str = reinterpret_cast<const wchar_t*>(
          [value cStringUsingEncoding:NSUTF32StringEncoding]);
      return true;
    }
  }
  return false;
}

std::wstring FileVersionInfo::GetStringValue(const wchar_t* name) {
  std::wstring str;
  if (GetValue(name, &str))
    return str;
  return std::wstring();
}
