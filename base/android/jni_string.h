// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_STRING_H_
#define BASE_ANDROID_JNI_STRING_H_

#include <jni.h>
#include <string>

#include "base/string16.h"
#include "base/string_piece.h"

namespace base {
namespace android {

// Convert a Java string to UTF8. Returns a std string.
std::string ConvertJavaStringToUTF8(JNIEnv* env, jstring str);

// Convert a std string to Java string.
jstring ConvertUTF8ToJavaString(JNIEnv* env, const base::StringPiece& str);

// Convert a Java string to UTF16. Returns a string16.
string16 ConvertJavaStringToUTF16(JNIEnv* env, jstring str);

// Convert a string16 to a Java string.
jstring ConvertUTF16ToJavaString(JNIEnv* env, const string16& str);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_STRING_H_
