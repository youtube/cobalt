// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkChangeNotifierAndroidTest;

class NetworkChangeNotifierAndroid : public NetworkChangeNotifier {
 public:
  virtual ~NetworkChangeNotifierAndroid();

  // Called from Java on the UI thread.
  void NotifyObserversOfConnectionTypeChange(
      JNIEnv* env, jobject obj, jint new_connection_type);
  jint GetConnectionType(JNIEnv* env, jobject obj);

  static bool Register(JNIEnv* env);

 private:
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierFactoryAndroid;

  NetworkChangeNotifierAndroid();

  void SetConnectionType(int connection_type);

  void ForceConnectivityState(bool state);

  // NetworkChangeNotifier:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE;

  base::android::ScopedJavaGlobalRef<jobject> java_network_change_notifier_;
  // TODO(pliard): http://crbug.com/150867. Use an atomic integer for the
  // connection type without the lock once a non-subtle atomic integer is
  // available under base/. That might never happen though.
  mutable base::Lock lock_;  // Protects the state below.
  // Written from the UI thread, read from any thread.
  int connection_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierAndroid);
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
