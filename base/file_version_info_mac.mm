// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info_mac.h"

#import <Foundation/Foundation.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "base/mac_util.h"

FileVersionInfoMac::FileVersionInfoMac(NSBundle *bundle) : bundle_(bundle) {
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  return CreateFileVersionInfo(mac_util::MainAppBundlePath());
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const FilePath& file_path) {
  NSString* path = base::SysUTF8ToNSString(file_path.value());
  NSBundle *bundle = [NSBundle bundleWithPath:path];
  NSString *identifier = [bundle bundleIdentifier];
  if (!identifier) {
    NOTREACHED() << "Unable to create file version for " << file_path.value();
    return NULL;
  } else {
    return new FileVersionInfoMac(bundle);
  }
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
  return GetWStringValue(kCFBundleNameKey);
}

std::wstring FileVersionInfoMac::product_short_name() {
  return GetWStringValue(kCFBundleNameKey);
}

std::wstring FileVersionInfoMac::comments() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::legal_copyright() {
  return GetWStringValue(CFSTR("CFBundleGetInfoString"));
}

std::wstring FileVersionInfoMac::product_version() {
  return GetWStringValue(CFSTR("CFBundleShortVersionString"));
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
  return GetWStringValue(kCFBundleNameKey);
}

std::wstring FileVersionInfoMac::special_build() {
  return std::wstring();
}

std::wstring FileVersionInfoMac::last_change() {
  return GetWStringValue(CFSTR("SVNRevision"));
}

bool FileVersionInfoMac::is_official_build() {
#if defined (GOOGLE_CHROME_BUILD)
  return true;
#else
  return false;
#endif
}

std::wstring FileVersionInfoMac::GetWStringValue(CFStringRef name) {
  if (bundle_) {
    NSString *ns_name = mac_util::CFToNSCast(name);
    NSString* value = [bundle_ objectForInfoDictionaryKey:ns_name];
    if (value) {
      return base::SysNSStringToWide(value);
    }
  }
  return std::wstring();
}
