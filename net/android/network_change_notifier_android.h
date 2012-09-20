// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkChangeNotifierAndroidTest;

class NetworkChangeNotifierAndroid : public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierAndroid();
  virtual ~NetworkChangeNotifierAndroid();

  void NotifyObserversOfConnectionTypeChange(JNIEnv* env, jobject obj);

  static bool Register(JNIEnv* env);

 private:
  friend class NetworkChangeNotifierAndroidTest;

  // NetworkChangeNotifier:
  virtual NetworkChangeNotifier::ConnectionType
      GetCurrentConnectionType() const OVERRIDE;

  void ForceConnectivityState(bool state);

  base::android::ScopedJavaGlobalRef<jobject> java_network_change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierAndroid);
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
