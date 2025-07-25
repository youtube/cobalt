// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_manager_impl.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_test_support.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr base::TimeDelta kDefaultSamplingIntervalForTesting =
    base::Milliseconds(10);

// Synchronous proxy to a device::mojom::PressureManager.
class PressureManagerImplSync {
 public:
  explicit PressureManagerImplSync(mojom::PressureManager* manager)
      : manager_(raw_ref<mojom::PressureManager>::from_ptr(manager)) {}
  ~PressureManagerImplSync() = default;

  PressureManagerImplSync(const PressureManagerImplSync&) = delete;
  PressureManagerImplSync& operator=(const PressureManagerImplSync&) = delete;

  mojom::PressureStatus AddClient(
      mojo::PendingRemote<mojom::PressureClient> client,
      mojom::PressureSource source) {
    base::test::TestFuture<mojom::PressureStatus> future;
    manager_->AddClient(std::move(client), source, future.GetCallback());
    return future.Get();
  }

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // device::mojom::PressureManager implementation is called synchronously,
  // so it's acceptable to rely on its own thread-safety checks.
  const raw_ref<mojom::PressureManager> manager_;
};

class FakePressureClient : public mojom::PressureClient {
 public:
  FakePressureClient() : client_(this) {}
  ~FakePressureClient() override = default;

  FakePressureClient(const FakePressureClient&) = delete;
  FakePressureClient& operator=(const FakePressureClient&) = delete;

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(device::mojom::PressureUpdatePtr update) override {
    updates_.emplace_back(*update);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  const std::vector<mojom::PressureUpdate>& updates() const { return updates_; }

  void SetNextUpdateCallback(base::OnceClosure callback) {
    CHECK(!update_callback_) << " already called before update received";
    update_callback_ = std::move(callback);
  }

  void WaitForUpdate() {
    base::RunLoop run_loop;
    SetNextUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  static void WaitForUpdates(
      std::initializer_list<FakePressureClient*> clients) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(clients.size(), run_loop.QuitClosure());
    for (FakePressureClient* client : clients)
      client->SetNextUpdateCallback(update_barrier);
    run_loop.Run();
  }

  mojo::PendingRemote<mojom::PressureClient> BindNewPipeAndPassRemote() {
    return client_.BindNewPipeAndPassRemote();
  }

 private:
  // Used to save PressureState.
  std::vector<mojom::PressureUpdate> updates_;

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_;

  mojo::Receiver<mojom::PressureClient> client_;
};

}  // namespace

class PressureManagerImplTest : public DeviceServiceTestBase {
 public:
  PressureManagerImplTest() = default;
  ~PressureManagerImplTest() override = default;

  PressureManagerImplTest(const PressureManagerImplTest&) = delete;
  PressureManagerImplTest& operator=(const PressureManagerImplTest&) = delete;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();

    manager_impl_ = PressureManagerImpl::Create();
    manager_impl_->SetCpuProbeForTesting(std::make_unique<FakeCpuProbe>(
        kDefaultSamplingIntervalForTesting,
        base::BindRepeating(&PressureManagerImpl::UpdateClients,
                            base::Unretained(manager_impl_.get()),
                            mojom::PressureSource::kCpu)));
    manager_.reset();
    manager_impl_->Bind(manager_.BindNewPipeAndPassReceiver());
    manager_impl_sync_ =
        std::make_unique<PressureManagerImplSync>(manager_.get());
  }

 protected:
  std::unique_ptr<PressureManagerImpl> manager_impl_;
  mojo::Remote<mojom::PressureManager> manager_;
  std::unique_ptr<PressureManagerImplSync> manager_impl_sync_;
};

TEST_F(PressureManagerImplTest, OneClient) {
  FakePressureClient client;
  ASSERT_EQ(manager_impl_sync_->AddClient(client.BindNewPipeAndPassRemote(),
                                          mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);

  client.WaitForUpdate();
  ASSERT_EQ(client.updates().size(), 1u);
  EXPECT_EQ(client.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client.updates()[0].state, mojom::PressureState::kFair);
}

TEST_F(PressureManagerImplTest, ThreeClients) {
  FakePressureClient client1;
  ASSERT_EQ(manager_impl_sync_->AddClient(client1.BindNewPipeAndPassRemote(),
                                          mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);
  FakePressureClient client2;
  ASSERT_EQ(manager_impl_sync_->AddClient(client2.BindNewPipeAndPassRemote(),
                                          mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);
  FakePressureClient client3;
  ASSERT_EQ(manager_impl_sync_->AddClient(client3.BindNewPipeAndPassRemote(),
                                          mojom::PressureSource::kCpu),
            mojom::PressureStatus::kOk);

  FakePressureClient::WaitForUpdates({&client1, &client2, &client3});
  ASSERT_EQ(client1.updates().size(), 1u);
  EXPECT_EQ(client1.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client1.updates()[0].state, mojom::PressureState::kFair);
  ASSERT_EQ(client2.updates().size(), 1u);
  EXPECT_EQ(client2.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client2.updates()[0].state, mojom::PressureState::kFair);
  ASSERT_EQ(client3.updates().size(), 1u);
  EXPECT_EQ(client3.updates()[0].source, mojom::PressureSource::kCpu);
  EXPECT_EQ(client3.updates()[0].state, mojom::PressureState::kFair);
}

TEST_F(PressureManagerImplTest, AddClientNoProbe) {
  manager_impl_->SetCpuProbeForTesting(nullptr);

  FakePressureClient client;
  ASSERT_EQ(manager_impl_sync_->AddClient(client.BindNewPipeAndPassRemote(),
                                          mojom::PressureSource::kCpu),
            mojom::PressureStatus::kNotSupported);
}

}  // namespace device
