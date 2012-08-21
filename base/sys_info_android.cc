// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <sys/system_properties.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"

namespace {

// Default version of Android to fall back to when actual version numbers
// cannot be acquired.
// TODO(dfalcantara): Keep this reasonably up to date with the latest publicly
//                    available version of Android.
static const int kDefaultAndroidMajorVersion = 4;
static const int kDefaultAndroidMinorVersion = 0;
static const int kDefaultAndroidBugfixVersion = 3;

// Parse out the OS version numbers from the system properties.
void ParseOSVersionNumbers(const char* os_version_str,
                           int32 *major_version,
                           int32 *minor_version,
                           int32 *bugfix_version) {
  if (os_version_str[0]) {
    // Try to parse out the version numbers from the string.
    int num_read = sscanf(os_version_str, "%d.%d.%d", major_version,
                          minor_version, bugfix_version);

    if (num_read > 0) {
      // If we don't have a full set of version numbers, make the extras 0.
      if (num_read < 2) *minor_version = 0;
      if (num_read < 3) *bugfix_version = 0;
      return;
    }
  }

  // For some reason, we couldn't parse the version number string.
  *major_version = kDefaultAndroidMajorVersion;
  *minor_version = kDefaultAndroidMinorVersion;
  *bugfix_version = kDefaultAndroidBugfixVersion;
}

int ParseHeapSize(const base::StringPiece& str) {
  const int64 KB = 1024;
  const int64 MB = 1024 * KB;
  const int64 GB = 1024 * MB;
  CHECK_GT(str.size(), 0u);
  int64 factor = 1;
  size_t length = str.size();
  if (str[length - 1] == 'k') {
    factor = KB;
    length--;
  } else if (str[length - 1] == 'm') {
    factor = MB;
    length--;
  } else if (str[length - 1] == 'g') {
    factor = GB;
    length--;
  } else {
    CHECK('0' <= str[length - 1] && str[length - 1] <= '9');
  }
  int64 result = 0;
  bool parsed = base::StringToInt64(str.substr(0, length), &result);
  CHECK(parsed);
  result = result * factor / MB;
  // dalvik.vm.heapsize property is writable by user,
  // truncate it to reasonable value to avoid overflows later.
  result = std::min<int64>(std::max<int64>(32, result), 1024);
  return static_cast<int>(result);
}

int GetDalvikHeapSizeMB() {
  char heap_size_str[PROP_VALUE_MAX];
  __system_property_get("dalvik.vm.heapsize", heap_size_str);
  return ParseHeapSize(heap_size_str);
}

}  // anonymous namespace

namespace base {

std::string SysInfo::GetAndroidBuildCodename() {
  char os_version_codename_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.codename", os_version_codename_str);
  return std::string(os_version_codename_str);
}

std::string SysInfo::GetAndroidBuildID() {
  char os_build_id_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.id", os_build_id_str);
  return std::string(os_build_id_str);
}

std::string SysInfo::GetDeviceName() {
  char device_model_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.model", device_model_str);
  return std::string(device_model_str);
}

void SysInfo::OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version) {
  // Read the version number string out from the properties.
  char os_version_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.release", os_version_str);

  // Parse out the numbers.
  ParseOSVersionNumbers(os_version_str, major_version, minor_version,
                        bugfix_version);
}

int SysInfo::DalvikHeapSizeMB() {
  static int heap_size = GetDalvikHeapSizeMB();
  return heap_size;
}

}  // namespace base
