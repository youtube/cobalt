// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_delegate_android.h"

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

NetworkChangeNotifierDelegateAndroid::NetworkChangeNotifierDelegateAndroid()
    : observers_(new ObserverListThreadSafe<Observer>()) {
  java_network_change_notifier_.Reset(
      Java_NetworkChangeNotifier_createInstance(
          base::android::AttachCurrentThread(),
          base::android::GetApplicationContext(),
          reinterpret_cast<jint>(this)));
}

NetworkChangeNotifierDelegateAndroid::~NetworkChangeNotifierDelegateAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_->AssertEmpty();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_destroyInstance(env);
}

void NetworkChangeNotifierDelegateAndroid::NotifyConnectionTypeChanged(
    JNIEnv* env,
    jobject obj,
    jint new_connection_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  connection_type_ = CheckConnectionType(new_connection_type) ?
      static_cast<ConnectionType>(new_connection_type) :
      NetworkChangeNotifier::CONNECTION_UNKNOWN;
  observers_->Notify(&Observer::OnConnectionTypeChanged, connection_type_);
}

jint NetworkChangeNotifierDelegateAndroid::GetConnectionType(JNIEnv*,
                                                             jobject) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return connection_type_;
}

void NetworkChangeNotifierDelegateAndroid::AddObserver(
    Observer* observer) {
  observers_->AddObserver(observer);
}

void NetworkChangeNotifierDelegateAndroid::RemoveObserver(
    Observer* observer) {
  observers_->RemoveObserver(observer);
}

void NetworkChangeNotifierDelegateAndroid::ForceConnectivityState(
    ConnectivityState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, state == ONLINE);
}

// static
bool NetworkChangeNotifierDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace net
