// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_android.h"

#include "base/logging.h"
#include "base/android/jni_android.h"
#include "jni/NetworkChangeNotifier_jni.h"

namespace net {

NetworkChangeNotifierAndroid::NetworkChangeNotifierAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_network_change_notifier_.Reset(
      Java_NetworkChangeNotifier_createInstance(
          env,
          base::android::GetApplicationContext(),
          reinterpret_cast<jint>(this)));
}

NetworkChangeNotifierAndroid::~NetworkChangeNotifierAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_destroyInstance(env);
  java_network_change_notifier_.Reset();
}

void NetworkChangeNotifierAndroid::NotifyObserversOfConnectionTypeChange(
    JNIEnv* env,
    jobject obj) {
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
}

void NetworkChangeNotifierAndroid::ForceConnectivityState(bool state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, state);
}

NetworkChangeNotifier::ConnectionType
    NetworkChangeNotifierAndroid::GetCurrentConnectionType() const {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Pull the connection type from the Java-side then convert it to a
  // native-side NetworkChangeNotifier::ConnectionType.
  jint connection_type = Java_NetworkChangeNotifier_connectionType(
      env, java_network_change_notifier_.obj());
  switch (connection_type) {
    case CONNECTION_UNKNOWN:
      return CONNECTION_UNKNOWN;
    case CONNECTION_ETHERNET:
      return CONNECTION_ETHERNET;
    case CONNECTION_WIFI:
      return CONNECTION_WIFI;
    case CONNECTION_2G:
      return CONNECTION_2G;
    case CONNECTION_3G:
      return CONNECTION_3G;
    case CONNECTION_4G:
      return CONNECTION_4G;
    case CONNECTION_NONE:
      return CONNECTION_NONE;
    default:
      NOTREACHED() << "Unknown connection type received: " << connection_type;
      return CONNECTION_NONE;
  }
}

// static
bool NetworkChangeNotifierAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace net
