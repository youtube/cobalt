// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

FileVersionInfoMac::FileVersionInfoMac(NSBundle *bundle) : bundle_(bundle) {
  [bundle_ retain];
}

FileVersionInfoMac::~FileVersionInfoMac() {
  [bundle_ release];
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  // TODO(erikkay): this should really use bundleForClass, but we don't have
  // a class to hang onto yet.
  NSBundle* bundle = [NSBundle mainBundle];
  return new FileVersionInfoMac(bundle);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const std::wstring& file_path) {
  NSString* path = [NSString stringWithCString:
      reinterpret_cast<const char*>(file_path.c_str())
        encoding:NSUTF32StringEncoding];
  return new FileVersionInfoMac([NSBundle bundleWithPath:path]);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const FilePath& file_path) {
  NSString* path = [NSString stringWithUTF8String:file_path.value().c_str()];
  return new FileVersionInfoMac([NSBundle bundleWithPath:path]);
}

std::wstring FileVersionInfoMac::company_name() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::company_short_name() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::internal_name() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::product_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfoMac::product_short_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfoMac::comments() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::legal_copyright() {
  return GetStringValue(L"CFBundleGetInfoString");
}

std::wstring FileVersionInfoMac::product_version() {
  return GetStringValue(L"CFBundleShortVersionString");
}

std::wstring FileVersionInfoMac::file_description() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::legal_trademarks() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::private_build() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::file_version() {
  return product_version();
}

std::wstring FileVersionInfoMac::original_filename() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfoMac::special_build() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::last_change() {
  return GetStringValue(L"SVNRevision");
}

bool FileVersionInfoMac::is_official_build() {
#if defined (GOOGLE_CHROME_BUILD)
  return true;
#else
  return false;
#endif
}

bool FileVersionInfoMac::GetValue(const wchar_t* name,
                                  std::wstring* value_str) {
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

std::wstring FileVersionInfoMac::GetStringValue(const wchar_t* name) {
  std::wstring str;
  if (GetValue(name, &str))
    return str;
  return std::wstring();
}
