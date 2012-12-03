// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

////////////////////////////////////////////////////////////////////////////////
// Threading considerations:
//
// This class is designed to meet various threading guarantees starting from the
// ones imposed by NetworkChangeNotifier:
// - The notifier can be constructed on any thread.
// - GetCurrentConnectionType() can be called on any thread.
//
// The fact that this implementation of NetworkChangeNotifier is backed by a
// Java side singleton class (see NetworkChangeNotifier.java) adds another
// threading constraint:
// - The calls to the Java side (stateful) object must be performed from a
//   single thread. This object happens to be a singleton which is used on the
//   application side on the main thread. Therefore all the method calls from
//   the native NetworkChangeNotifierAndroid class to its Java counterpart are
//   performed on the main thread.
//
// This leads to a design involving the following native classes:
// 1) NetworkChangeNotifierFactoryAndroid ('factory')
// 2) NetworkChangeNotifierDelegateAndroid ('delegate')
// 3) NetworkChangeNotifierAndroid ('notifier')
//
// The factory constructs and owns the delegate. The factory is constructed and
// destroyed on the main thread which makes it construct and destroy the
// delegate on the main thread too. This guarantees that the calls to the Java
// side are performed on the main thread.
// Note that after the factory's construction, the factory's creation method can
// be called from any thread since the delegate's construction (performing the
// JNI calls) already happened on the main thread (when the factory was
// constructed).
//
////////////////////////////////////////////////////////////////////////////////
// Propagation of network change notifications:
//
// When the factory is requested to create a new instance of the notifier, the
// factory passes the delegate to the notifier (without transferring ownership).
// Note that there is a one-to-one mapping between the factory and the
// delegate as explained above. But the factory naturally creates multiple
// instances of the notifier. That means that there is a one-to-many mapping
// between delegate and notifier (i.e. a single delegate can be shared by
// multiple notifiers).
// At construction the notifier (which is also an observer) subscribes to
// notifications fired by the delegate. These notifications, received by the
// delegate (and forwarded to the notifier(s)), are sent by the Java side
// notifier (see NetworkChangeNotifier.java) and are initiated by the Android
// platform.
// Notifications from the Java side always arrive on the main thread. The
// delegate then forwards these notifications to the threads of each observer
// (network change notifier). The network change notifier than processes the
// state change, and notifies each of its observers on their threads.
//
// This can also be seen as:
// Android platform -> NetworkChangeNotifier (Java) ->
// NetworkChangeNotifierDelegateAndroid -> NetworkChangeNotifierAndroid.

#include "net/android/network_change_notifier_android.h"

namespace net {

NetworkChangeNotifierAndroid::~NetworkChangeNotifierAndroid() {
  delegate_->RemoveObserver(this);
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierAndroid::GetCurrentConnectionType() const {
  base::AutoLock auto_lock(connection_type_lock_);
  return connection_type_;
}

void NetworkChangeNotifierAndroid::OnConnectionTypeChanged(
    ConnectionType new_connection_type) {
  SetConnectionType(new_connection_type);
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
}

// static
bool NetworkChangeNotifierAndroid::Register(JNIEnv* env) {
  return NetworkChangeNotifierDelegateAndroid::Register(env);
}

NetworkChangeNotifierAndroid::NetworkChangeNotifierAndroid(
    NetworkChangeNotifierDelegateAndroid* delegate)
    : delegate_(delegate) {
  SetConnectionType(NetworkChangeNotifier::CONNECTION_UNKNOWN);
  delegate_->AddObserver(this);
}

void NetworkChangeNotifierAndroid::SetConnectionType(
    ConnectionType new_connection_type) {
  base::AutoLock auto_lock(connection_type_lock_);
  connection_type_ = new_connection_type;
}

}  // namespace net
