// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_android.h"

#include "base/message_loop.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestConnectionTypeObserver :
    public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  TestConnectionTypeObserver() :
      times_connection_type_has_been_changed_(0),
      current_connection_(NetworkChangeNotifier::CONNECTION_UNKNOWN) {
  }

  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) {
    times_connection_type_has_been_changed_++;
    current_connection_ = type;
  }

  int times_connection_type_has_been_changed() const {
    return times_connection_type_has_been_changed_;
  }

  NetworkChangeNotifier::ConnectionType current_connection() const {
    return current_connection_;
  }

 private:
  int times_connection_type_has_been_changed_;
  NetworkChangeNotifier::ConnectionType current_connection_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionTypeObserver);
};

}  // namespace

class NetworkChangeNotifierAndroidTest : public testing::Test {
 public:
  NetworkChangeNotifierAndroidTest() : connection_type_observer_(NULL) {
  }

  void ForceConnectivityState(bool state) {
    notifier_->ForceConnectivityState(state);
  }

  const TestConnectionTypeObserver* observer() const {
    return connection_type_observer_.get();
  }

 protected:
  virtual void SetUp() {
    notifier_.reset(new NetworkChangeNotifierAndroid());
    connection_type_observer_.reset(new TestConnectionTypeObserver());
    NetworkChangeNotifier::AddConnectionTypeObserver(
        connection_type_observer_.get());
  }

 private:
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  scoped_ptr<NetworkChangeNotifierAndroid> notifier_;
  scoped_ptr<TestConnectionTypeObserver> connection_type_observer_;
};


TEST_F(NetworkChangeNotifierAndroidTest, ObserverNotified) {
  // This test exercises JNI calls between the native-side
  // NetworkChangeNotifierAndroid and java-side NetworkChangeNotifier.
  EXPECT_EQ(0, observer()->times_connection_type_has_been_changed());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
      observer()->current_connection());

  ForceConnectivityState(false);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(1, observer()->times_connection_type_has_been_changed());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
      observer()->current_connection());

  ForceConnectivityState(false);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(1, observer()->times_connection_type_has_been_changed());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
      observer()->current_connection());

  ForceConnectivityState(true);
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(2, observer()->times_connection_type_has_been_changed());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
      observer()->current_connection());
}

}  // namespace net
