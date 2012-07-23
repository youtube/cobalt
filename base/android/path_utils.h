// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_PATH_UTILS_H_
#define BASE_ANDROID_PATH_UTILS_H_

#include <jni.h>
#include <string>

namespace base {
namespace android {

// Return the absolute path to the data directory of the current application.
// This method is dedicated for base_paths_android.c, Using
// PathService::Get(base::DIR_ANDROID_APP_DATA, ...) gets the data dir.
std::string GetDataDirectory();

// Return the absolute path to the cache directory. This method is dedicated for
// base_paths_android.c, Using PathService::Get(base::DIR_CACHE, ...) gets the
// cache dir.
std::string GetCacheDirectory();

// Returns the path to the public downloads directory.
std::string GetDownloadsDirectory();

// Returns the path to the native JNI libraries via
// ApplicationInfo.nativeLibraryDir on the Java side.
std::string GetNativeLibraryDirectory();

bool RegisterPathUtils(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_PATH_UTILS_H_
