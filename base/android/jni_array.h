// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ARRAY_H_
#define BASE_ANDROID_JNI_ARRAY_H_

#include <jni.h>
#include <string>
#include <vector>

namespace base {
namespace android {

// Returns a new Java byte array converted from the given bytes array.
jbyteArray ToJavaByteArray(JNIEnv* env, const unsigned char* bytes, size_t len);

// Returns a array of Java byte array converted from |v|.
jobjectArray ToJavaArrayOfByteArray(JNIEnv* env,
                                    const std::vector<std::string>& v);

jobjectArray ToJavaArrayOfStrings(JNIEnv* env,
                                  const std::vector<std::string>& v);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ARRAY_H_
