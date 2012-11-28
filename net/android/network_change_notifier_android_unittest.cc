// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See network_change_notifier_android.h for design explanations.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/android/network_change_notifier_android.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Template used to generate both the NetworkChangeNotifierDelegateAndroid and
// NetworkChangeNotifier::ConnectionTypeObserver implementations which have the
// same interface.
template <typename BaseObserver>
class ObserverImpl : public BaseObserver {
 public:
  ObserverImpl()
      : times_connection_type_changed_(0),
        current_connection_(NetworkChangeNotifier::CONNECTION_UNKNOWN) {
  }

  // BaseObserver:
  virtual void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) OVERRIDE {
    times_connection_type_changed_++;
    current_connection_ = type;
  }

  int times_connection_type_changed() const {
    return times_connection_type_changed_;
  }

  NetworkChangeNotifier::ConnectionType current_connection() const {
    return current_connection_;
  }

 private:
  int times_connection_type_changed_;
  NetworkChangeNotifier::ConnectionType current_connection_;

  DISALLOW_COPY_AND_ASSIGN(ObserverImpl);
};

}  // namespace

class BaseNetworkChangeNotifierAndroidTest : public testing::Test {
 protected:
  typedef NetworkChangeNotifier::ConnectionType ConnectionType;

  virtual ~BaseNetworkChangeNotifierAndroidTest() {}

  void RunTest(
      const base::Callback<int(void)>& times_connection_type_changed_callback,
      const base::Callback<ConnectionType(void)>&  connection_type_getter) {
    EXPECT_EQ(0, times_connection_type_changed_callback.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
              connection_type_getter.Run());

    ForceConnectivityState(NetworkChangeNotifierDelegateAndroid::OFFLINE);
    EXPECT_EQ(1, times_connection_type_changed_callback.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
              connection_type_getter.Run());

    ForceConnectivityState(NetworkChangeNotifierDelegateAndroid::OFFLINE);
    EXPECT_EQ(1, times_connection_type_changed_callback.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
              connection_type_getter.Run());

    ForceConnectivityState(NetworkChangeNotifierDelegateAndroid::ONLINE);
    EXPECT_EQ(2, times_connection_type_changed_callback.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
              connection_type_getter.Run());
  }

  void ForceConnectivityState(
      NetworkChangeNotifierDelegateAndroid::ConnectivityState state) {
    delegate_.ForceConnectivityState(state);
    // Note that this is needed because ObserverListThreadSafe uses PostTask().
    MessageLoop::current()->RunUntilIdle();
  }

  NetworkChangeNotifierDelegateAndroid delegate_;
};

class NetworkChangeNotifierDelegateAndroidTest
    : public BaseNetworkChangeNotifierAndroidTest {
 protected:
  typedef ObserverImpl<
      NetworkChangeNotifierDelegateAndroid::Observer> TestDelegateObserver;

  NetworkChangeNotifierDelegateAndroidTest() {
    delegate_.AddObserver(&delegate_observer_);
    delegate_.AddObserver(&other_delegate_observer_);
  }

  virtual ~NetworkChangeNotifierDelegateAndroidTest() {
    delegate_.RemoveObserver(&delegate_observer_);
    delegate_.RemoveObserver(&other_delegate_observer_);
  }

  TestDelegateObserver delegate_observer_;
  TestDelegateObserver other_delegate_observer_;
};

// Tests that the NetworkChangeNotifierDelegateAndroid's observers are notified.
// A testing-only observer is used here for testing. In production the
// delegate's observers are instances of NetworkChangeNotifierAndroid.
TEST_F(NetworkChangeNotifierDelegateAndroidTest, DelegateObserverNotified) {
  // Test the logic with a single observer.
  RunTest(
      base::Bind(
          &TestDelegateObserver::times_connection_type_changed,
          base::Unretained(&delegate_observer_)),
      base::Bind(
          &TestDelegateObserver::current_connection,
          base::Unretained(&delegate_observer_)));
  // Check that *all* the observers are notified. Both observers should have the
  // same state.
  EXPECT_EQ(delegate_observer_.times_connection_type_changed(),
            other_delegate_observer_.times_connection_type_changed());
  EXPECT_EQ(delegate_observer_.current_connection(),
            other_delegate_observer_.current_connection());
}

class NetworkChangeNotifierAndroidTest
    : public BaseNetworkChangeNotifierAndroidTest {
 protected:
  typedef ObserverImpl<
      NetworkChangeNotifier::ConnectionTypeObserver> TestConnectionTypeObserver;

  NetworkChangeNotifierAndroidTest() : notifier_(&delegate_) {
    NetworkChangeNotifier::AddConnectionTypeObserver(
        &connection_type_observer_);
    NetworkChangeNotifier::AddConnectionTypeObserver(
        &other_connection_type_observer_);
  }

  TestConnectionTypeObserver connection_type_observer_;
  TestConnectionTypeObserver other_connection_type_observer_;
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  NetworkChangeNotifierAndroid notifier_;
};

// When a NetworkChangeNotifierAndroid is observing a
// NetworkChangeNotifierDelegateAndroid for network state changes, and the
// NetworkChangeNotifierDelegateAndroid's connectivity state changes, the
// NetworkChangeNotifierAndroid should reflect that state.
TEST_F(NetworkChangeNotifierAndroidTest,
       NotificationsSentToNetworkChangeNotifierAndroid) {
  RunTest(
      base::Bind(
          &TestConnectionTypeObserver::times_connection_type_changed,
          base::Unretained(&connection_type_observer_)),
      base::Bind(
          &NetworkChangeNotifierAndroid::GetCurrentConnectionType,
          base::Unretained(&notifier_)));
}

// When a NetworkChangeNotifierAndroid's connection state changes, it should
// notify all of its observers.
TEST_F(NetworkChangeNotifierAndroidTest,
       NotificationsSentToClientsOfNetworkChangeNotifier) {
  RunTest(
      base::Bind(
          &TestConnectionTypeObserver::times_connection_type_changed,
          base::Unretained(&connection_type_observer_)),
      base::Bind(
          &TestConnectionTypeObserver::current_connection,
          base::Unretained(&connection_type_observer_)));
  // Check that *all* the observers are notified.
  EXPECT_EQ(connection_type_observer_.times_connection_type_changed(),
            other_connection_type_observer_.times_connection_type_changed());
  EXPECT_EQ(connection_type_observer_.current_connection(),
            other_connection_type_observer_.current_connection());
}

}  // namespace net
