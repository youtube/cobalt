// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkChangeNotifierAndroidTest;
class NetworkChangeNotifierFactoryAndroid;

// NetworkChangeNotifierAndroid observes network events from the Android
// notification system and forwards them to observers.
//
// The implementation is complicated by the differing lifetime and thread
// affinity requirements of Android notifications and of NetworkChangeNotifier.
//
// High-level overview:
// NetworkChangeNotifier.java - Receives notifications from Android system, and
// notifies native code via JNI (on the main application thread).
// NetworkChangeNotifierDelegateAndroid ('Delegate') - Listens for notifications
//   sent via JNI on the main application thread, and forwards them to observers
//   on their threads. Owned by Factory, lives exclusively on main application
//   thread.
// NetworkChangeNotifierFactoryAndroid ('Factory') - Creates the Delegate on the
//   main thread to receive JNI events, and vends Notifiers. Lives exclusively
//   on main application thread, and outlives all other classes.
// NetworkChangeNotifierAndroid ('Notifier') - Receives event notifications from
//   the Delegate. Processes and forwards these events to the
//   NetworkChangeNotifier observers on their threads. May live on any thread
//   and be called by any thread.
//
// For more details, see the implementation file.
class NET_EXPORT_PRIVATE NetworkChangeNotifierAndroid
    : public NetworkChangeNotifier,
      public NetworkChangeNotifierDelegateAndroid::Observer {
 public:
  virtual ~NetworkChangeNotifierAndroid();

  // NetworkChangeNotifier:
  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE;

  // NetworkChangeNotifierDelegateAndroid::Observer:
  virtual void OnConnectionTypeChanged(
      ConnectionType new_connection_type) OVERRIDE;

  static bool Register(JNIEnv* env);

 private:
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierFactoryAndroid;

  explicit NetworkChangeNotifierAndroid(
      NetworkChangeNotifierDelegateAndroid* delegate);

  void SetConnectionType(ConnectionType new_connection_type);

  NetworkChangeNotifierDelegateAndroid* const delegate_;
  mutable base::Lock connection_type_lock_;  // Protects the state below.
  ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierAndroid);
};

}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
