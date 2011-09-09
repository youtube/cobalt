// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCOPED_JAVA_REF_H_
#define BASE_ANDROID_SCOPED_JAVA_REF_H_

#include <jni.h>
#include <stddef.h>

#include "base/logging.h"

namespace base {
namespace android {

// Forward declare the generic java reference template class.
template<typename T> class JavaRef;

// Template specialization of JavaRef, which acts as the base class for all
// other JavaRef<> template types. This allows you to e.g. pass
// ScopedJavaLocalRef<jstring> into a function taking const JavaRef<jobject>&
template<>
class JavaRef<jobject> {
 public:
  JNIEnv* env() const { return env_; }
  jobject obj() const { return obj_; }

 protected:
  // Initializes a NULL reference.
  JavaRef();

  // Takes ownership of the |obj| reference passed; requires it to be a local
  // reference type.
  JavaRef(JNIEnv* env, jobject obj);

  ~JavaRef();

  // The following are implementation detail convenience methods, for
  // use by the sub-classes.
  void SetNewLocalRef(JNIEnv* env, jobject obj);
  void SetNewGlobalRef(JNIEnv* env, jobject obj);
  jobject ReleaseInternal();

 private:
  JNIEnv* env_;
  jobject obj_;

  DISALLOW_COPY_AND_ASSIGN(JavaRef);
};

// Generic base class for ScopedJavaLocalRef and ScopedJavaGlobalRef. Useful
// for allowing functions to accept a reference without having to mandate
// whether it is a local or global type.
template<typename T>
class JavaRef : public JavaRef<jobject> {
 public:
  T obj() const { return static_cast<T>(JavaRef<jobject>::obj()); }

 protected:
  JavaRef() {}
  ~JavaRef() {}

  JavaRef(JNIEnv* env, T obj) : JavaRef<jobject>(env, obj) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(JavaRef);
};

// Holds a local reference to a Java object. The local reference is scoped
// to the lifetime of this object. Note that since a JNI Env is only suitable
// for use on a single thread, objects of this class must be created, used and
// destroyed on the same thread.
// In general, this class should only be used as a stack-based object. If you
// wish to have the reference outlive the current callstack (e.g. as a class
// member) use ScopedJavaGlobalRef instead.
template<typename T>
class ScopedJavaLocalRef : public JavaRef<T> {
 public:
  ScopedJavaLocalRef() {}

  // Non-explicit copy constructor, to allow ScopedJavaLocalRef to be returned
  // by value as this is the normal usage pattern.
  ScopedJavaLocalRef(const ScopedJavaLocalRef<T>& other) {
    this->Reset(other);
  }

  template<typename U>
  explicit ScopedJavaLocalRef(const U& other) {
    this->Reset(other);
  }

  // Assumes that |obj| is a local reference to a Java object and takes
  // ownership  of this local reference.
  ScopedJavaLocalRef(JNIEnv* env, T obj) : JavaRef<T>(env, obj) {}

  ~ScopedJavaLocalRef() {
    this->Reset();
  }

  // Overloaded assignment operator defined for consistency with the implicit
  // copy constructor.
  void operator=(const ScopedJavaLocalRef<T>& other) {
    this->Reset(other);
  }

  void Reset() {
    this->SetNewLocalRef(NULL, NULL);
  }

  template<typename U>
  void Reset(const U& other) {
    this->Reset(other.env(), other.obj());
  }

  template<typename U>
  void Reset(JNIEnv* env, U obj) {
    implicit_cast<T>(obj);  // Ensure U is assignable to T
    this->SetNewLocalRef(env, obj);
  }

  // Releases the local reference to the caller. The caller *must* delete the
  // local reference when it is done with it.
  T Release() {
    return static_cast<T>(this->ReleaseInternal());
  }
};

// Holds a global reference to a Java object. The global reference is scoped
// to the lifetime of this object. Note that since a JNI Env is only suitable
// for use on a single thread, objects of this class must be created, used and
// destroyed on the same thread.
template<typename T>
class ScopedJavaGlobalRef : public JavaRef<T> {
 public:
  ScopedJavaGlobalRef() {}

  explicit ScopedJavaGlobalRef(const ScopedJavaGlobalRef<T>& other) {
    this->Reset(other);
  }

  template<typename U>
  explicit ScopedJavaGlobalRef(const U& other) {
    this->Reset(other);
  }

  ~ScopedJavaGlobalRef() {
    this->Reset();
  }

  void Reset() {
    this->SetNewGlobalRef(NULL, NULL);
  }

  template<typename U>
  void Reset(const U& other) {
    this->Reset(other.env(), other.obj());
  }

  template<typename U>
  void Reset(JNIEnv* env, U obj) {
    implicit_cast<T>(obj);  // Ensure U is assignable to T
    this->SetNewGlobalRef(env, obj);
  }

  // Releases the global reference to the caller. The caller *must* delete the
  // global reference when it is done with it.
  T Release() {
    return static_cast<T>(this->ReleaseInternal());
  }
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_SCOPED_JAVA_REF_H_
