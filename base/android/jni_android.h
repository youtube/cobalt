// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ANDROID_H_
#define BASE_ANDROID_JNI_ANDROID_H_

#include <jni.h>
#include <sys/types.h>

namespace base {
namespace android {

// Attach the current thread to the VM (if necessary) and return the JNIEnv*.
JNIEnv* AttachCurrentThread();

// Detach the current thread from VM if it is attached.
void DetachFromVM();

// Initializes the global JVM. It is not necessarily called before
// InitApplicationContext().
void InitVM(JavaVM* vm);

// Initializes the global application context object. The |context| should be
// the global reference of application context object.  It is not necessarily
// called after InitVM().
// TODO: We might combine InitVM() and InitApplicationContext() into one method.
void InitApplicationContext(jobject context);

// Returns the application context assigned by InitApplicationContext().
jobject GetApplicationContext();

// Gets the method ID from the class name. Clears the pending Java exception
// and returns NULL if the method is not found. Caches results. Note that
// GetMethodID() below avoids a class lookup, but does not cache results.
// Strings passed to this function are held in the cache and MUST remain valid
// beyond the duration of all future calls to this function, across all
// threads. In practice, this means that the function should only be used with
// string constants.
jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                   const char* class_name,
                                   const char* method,
                                   const char* jni_signature);

// Get the method ID for a method. Will clear the pending Java
// exception and return 0 if the method is not found.
jmethodID GetMethodID(JNIEnv* env,
                      jclass clazz,
                      const char* const method,
                      const char* const jni_signature);

// Get the method ID for a class static method. Will clear the pending Java
// exception and return 0 if the method is not found.
jmethodID GetStaticMethodID(JNIEnv* env,
                            jclass clazz,
                            const char* const method,
                            const char* const jni_signature);

// Gets the field ID for a class field. Clears the pending Java exception and
// returns NULL if the field is not found.
jfieldID GetFieldID(JNIEnv* env,
                    jclass clazz,
                    const char* field,
                    const char* jni_signature);

// Returns true if an exception is pending in the provided JNIEnv*. If an
// exception is pending, this function prints and then clears it.
bool CheckException(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ANDROID_H_
