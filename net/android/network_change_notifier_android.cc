// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "jni/NetworkChangeNotifier_jni.h"

namespace net {

namespace {

// Returns whether the provided connection type is known.
bool CheckConnectionType(int connection_type) {
  switch (connection_type) {
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
    case NetworkChangeNotifier::CONNECTION_WIFI:
    case NetworkChangeNotifier::CONNECTION_2G:
    case NetworkChangeNotifier::CONNECTION_3G:
    case NetworkChangeNotifier::CONNECTION_4G:
    case NetworkChangeNotifier::CONNECTION_NONE:
      return true;
    default:
      NOTREACHED() << "Unknown connection type received: " << connection_type;
      return false;
  }
}

}  // namespace

NetworkChangeNotifierAndroid::~NetworkChangeNotifierAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_destroyInstance(env);
}

void NetworkChangeNotifierAndroid::NotifyObserversOfConnectionTypeChange(
    JNIEnv* env,
    jobject obj,
    jint new_connection_type) {
  int connection_type = CheckConnectionType(new_connection_type) ?
      new_connection_type : CONNECTION_UNKNOWN;
  SetConnectionType(connection_type);
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
}

jint NetworkChangeNotifierAndroid::GetConnectionType(JNIEnv* env, jobject obj) {
  return GetCurrentConnectionType();
}

// static
bool NetworkChangeNotifierAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

NetworkChangeNotifierAndroid::NetworkChangeNotifierAndroid() {
  SetConnectionType(CONNECTION_UNKNOWN);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_network_change_notifier_.Reset(
      Java_NetworkChangeNotifier_createInstance(
          env,
          base::android::GetApplicationContext(),
          reinterpret_cast<jint>(this)));
}

void NetworkChangeNotifierAndroid::SetConnectionType(int connection_type) {
  base::AutoLock auto_lock(lock_);
  connection_type_ = connection_type;
}

void NetworkChangeNotifierAndroid::ForceConnectivityState(bool state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, state);
}

NetworkChangeNotifier::ConnectionType
    NetworkChangeNotifierAndroid::GetCurrentConnectionType() const {
  base::AutoLock auto_lock(lock_);
  return static_cast<NetworkChangeNotifier::ConnectionType>(connection_type_);
}

}  // namespace net
