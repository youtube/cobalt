// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/featured/featured_client.h"
#include "base/check_op.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash::featured {

class FeaturedClientTest : public testing::Test {
 public:
  FeaturedClientTest()
      : bus_(base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options{})),
        path_(dbus::ObjectPath(::featured::kFeaturedServicePath)),
        proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            bus_.get(),
            ::featured::kFeaturedServiceName,
            path_)) {
    // Makes sure `GetObjectProxy()` is called with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::featured::kFeaturedServiceName, path_))
        .WillRepeatedly(Return(proxy_.get()));
  }

  FeaturedClientTest(const FeaturedClientTest&) = delete;
  FeaturedClientTest& operator=(const FeaturedClientTest&) = delete;

  ~FeaturedClientTest() override = default;

 protected:
  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  dbus::ObjectPath path_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;
};

TEST_F(FeaturedClientTest, InitializeSuccess) {
  FeaturedClient::Initialize(bus_.get());

  ASSERT_NE(FeaturedClient::Get(), nullptr);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, NotInitializedGet) {
  ASSERT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, InitializeFakeSuccess) {
  FeaturedClient::InitializeFake();

  ASSERT_NE(FakeFeaturedClient::Get(), nullptr);

  FakeFeaturedClient::Shutdown();

  EXPECT_EQ(FakeFeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, NotInitializedFakeGet) {
  ASSERT_EQ(FakeFeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientTest, HandleSeedFetched_Success) {
  EXPECT_CALL(*proxy_,
              DoCallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke([](dbus::MethodCall* call, int timeout_ms,
                          dbus::MockObjectProxy::ResponseCallback* callback) {
        std::unique_ptr<dbus::Response> response =
            dbus::Response::CreateEmpty();
        std::move(*callback).Run(response.get());
      }));

  FeaturedClient::Initialize(bus_.get());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_TRUE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

// Check that `HandleSeedFetched` runs the callback with a false success value
// if the server (platform) returns an error responses.
TEST_F(FeaturedClientTest, HandleSeedFetched_Failure_ErrorResponse) {
  EXPECT_CALL(*proxy_,
              DoCallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke([](dbus::MethodCall* call, int timeout_ms,
                          dbus::MockObjectProxy::ResponseCallback* callback) {
        // Not setting the serial causes a crash.
        call->SetSerial(123);
        std::unique_ptr<dbus::Response> response =
            dbus::ErrorResponse::FromMethodCall(call, DBUS_ERROR_FAILED,
                                                "test");
        std::move(*callback).Run(response.get());
      }));

  FeaturedClient::Initialize(bus_.get());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_FALSE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

// Check that `HandleSeedFetched` runs the callback with a false success value
// if the method call is unsuccessful (response is a nullptr).
TEST_F(FeaturedClientTest, HandleSeedFetched_Failure_NullResponse) {
  EXPECT_CALL(*proxy_,
              DoCallMethod(_, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke([](dbus::MethodCall* call, int timeout_ms,
                          dbus::MockObjectProxy::ResponseCallback* callback) {
        std::move(*callback).Run(nullptr);
      }));

  FeaturedClient::Initialize(bus_.get());
  FeaturedClient* client = FeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_FALSE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

// Check that Fake runs `HandleSeedFetched` callback with a false success value
// by default (no expected responses added).
TEST_F(FeaturedClientTest, FakeHandleSeedFetched_InvokeFalseByDefault) {
  FeaturedClient::InitializeFake();
  FakeFeaturedClient* client = FakeFeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_FALSE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 1);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FakeFeaturedClient::Get(), nullptr);
}

// Check that Fake runs `HandleSeedFetched` callback with value added by
// `AddResponse`.
TEST_F(FeaturedClientTest, FakeHandleSeedFetched_InvokeSuccessWhenSet) {
  FeaturedClient::InitializeFake();
  FakeFeaturedClient* client = FakeFeaturedClient::Get();

  ASSERT_NE(client, nullptr);

  ::featured::SeedDetails safe_seed;
  client->AddResponse(true);

  bool ran_callback = false;
  client->HandleSeedFetched(
      safe_seed, base::BindLambdaForTesting([&ran_callback](bool success) {
        EXPECT_TRUE(success);
        ran_callback = true;
      }));
  // Ensures the callback was executed.
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 1);

  FeaturedClient::Shutdown();

  EXPECT_EQ(FakeFeaturedClient::Get(), nullptr);
}

#if DCHECK_IS_ON()
using FeaturedClientDeathTest = FeaturedClientTest;
TEST_F(FeaturedClientDeathTest, InitializeFailure_NullBus) {
  EXPECT_DEATH(FeaturedClient::Initialize(nullptr), "");
}

TEST_F(FeaturedClientDeathTest, DoubleInitialize) {
  FeaturedClient::Initialize(bus_.get());

  EXPECT_DEATH(FeaturedClient::Initialize(bus_.get()), "");

  FeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}

TEST_F(FeaturedClientDeathTest, DoubleInitializeFake) {
  FeaturedClient::InitializeFake();

  EXPECT_DEATH(FeaturedClient::InitializeFake(), "");

  FakeFeaturedClient::Shutdown();

  EXPECT_EQ(FeaturedClient::Get(), nullptr);
}
#endif  // DCHECK_IS_ON()
}  // namespace ash::featured
