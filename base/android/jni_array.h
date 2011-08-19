// Copyright 2011 Google Inc. All Rights Reserved.
// Author: michaelbai@google.com (Tao Bai)


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

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ARRAY_H_
