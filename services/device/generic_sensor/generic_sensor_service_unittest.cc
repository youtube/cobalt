// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service.h"
#include "services/device/device_service_test_base.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

using ::testing::_;
using ::testing::Invoke;

namespace device {

using mojom::SensorType;

namespace {

void CheckValue(double expect, double value) {
  EXPECT_DOUBLE_EQ(expect, value);
}

void CheckSuccess(base::OnceClosure quit_closure,
                  bool expect,
                  bool is_success) {
  EXPECT_EQ(expect, is_success);
  std::move(quit_closure).Run();
}

class TestSensorClient : public mojom::SensorClient {
 public:
  TestSensorClient(SensorType type) : type_(type) {}

  TestSensorClient(const TestSensorClient&) = delete;
  TestSensorClient& operator=(const TestSensorClient&) = delete;

  // Implements mojom::SensorClient:
  void SensorReadingChanged() override {
    if (!shared_buffer_reader_->GetReading(&reading_data_)) {
      ADD_FAILURE() << "Failed to get readings from shared buffer";
      return;
    }
    if (check_value_)
      std::move(check_value_).Run(reading_data_.als.value);
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }
  void RaiseError() override {}

  // Sensor mojo interfaces callbacks:
  void OnSensorCreated(base::OnceClosure quit_closure,
                       mojom::SensorCreationResult result,
                       mojom::SensorInitParamsPtr params) {
    ASSERT_TRUE(params);
    EXPECT_EQ(mojom::SensorCreationResult::SUCCESS, result);
    EXPECT_TRUE(params->memory.IsValid());
    const double expected_default_frequency =
        std::min(30.0, GetSensorMaxAllowedFrequency(type_));
    EXPECT_DOUBLE_EQ(expected_default_frequency,
                     params->default_configuration.frequency());
    const double expected_maximum_frequency =
        std::min(50.0, GetSensorMaxAllowedFrequency(type_));
    EXPECT_DOUBLE_EQ(expected_maximum_frequency, params->maximum_frequency);
    EXPECT_DOUBLE_EQ(1.0, params->minimum_frequency);

    shared_buffer_reader_ = device::SensorReadingSharedBufferReader::Create(
        std::move(params->memory), params->buffer_offset);
    ASSERT_TRUE(shared_buffer_reader_);

    sensor_.Bind(std::move(params->sensor));
    client_receiver_.Bind(std::move(params->client_receiver));
    std::move(quit_closure).Run();
  }

  void OnGetDefaultConfiguration(
      base::OnceClosure quit_closure,
      const PlatformSensorConfiguration& configuration) {
    EXPECT_DOUBLE_EQ(30.0, configuration.frequency());
    std::move(quit_closure).Run();
  }

  void OnAddConfiguration(base::OnceCallback<void(bool)> expect_function,
                          bool is_success) {
    std::move(expect_function).Run(is_success);
  }

  // For SensorReadingChanged().
  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  void SetCheckValueCallback(base::OnceCallback<void(double)> callback) {
    check_value_ = std::move(callback);
  }

  mojom::Sensor* sensor() { return sensor_.get(); }
  void ResetSensor() { sensor_.reset(); }

 private:
  mojo::Remote<mojom::Sensor> sensor_;
  mojo::Receiver<mojom::SensorClient> client_receiver_{this};
  std::unique_ptr<device::SensorReadingSharedBufferReader>
      shared_buffer_reader_;
  SensorReading reading_data_;

  // Test Clients set |quit_closure_| and start a RunLoop in main thread, then
  // expect the |quit_closure| will quit the RunLoop in SensorReadingChanged().
  // In this way we guarantee the SensorReadingChanged() does be triggered.
  base::OnceClosure quit_closure_;

  // |check_value_| is called to verify the data is same as we
  // expected in SensorReadingChanged().
  base::OnceCallback<void(double)> check_value_;
  SensorType type_;
};

class GenericSensorServiceTest : public DeviceServiceTestBase {
 public:
  GenericSensorServiceTest() = default;

  GenericSensorServiceTest(const GenericSensorServiceTest&) = delete;
  GenericSensorServiceTest& operator=(const GenericSensorServiceTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kGenericSensorExtraClasses}, {});
    DeviceServiceTestBase::SetUp();

    fake_platform_sensor_provider_ = new FakePlatformSensorProvider();
    device_service_impl()->SetPlatformSensorProviderForTesting(
        base::WrapUnique(fake_platform_sensor_provider_.get()));
    device_service()->BindSensorProvider(
        sensor_provider_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::SensorProvider> sensor_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // This object is owned by the DeviceService instance.
  raw_ptr<FakePlatformSensorProvider> fake_platform_sensor_provider_;
};

// Requests the SensorProvider to create a sensor.
TEST_F(GenericSensorServiceTest, GetSensorTest) {
  auto client = std::make_unique<TestSensorClient>(SensorType::PROXIMITY);
  base::RunLoop run_loop;
  sensor_provider_->GetSensor(
      SensorType::PROXIMITY,
      base::BindOnce(&TestSensorClient::OnSensorCreated,
                     base::Unretained(client.get()), run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests GetDefaultConfiguration.
TEST_F(GenericSensorServiceTest, GetDefaultConfigurationTest) {
  auto client = std::make_unique<TestSensorClient>(SensorType::ACCELEROMETER);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::ACCELEROMETER,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    client->sensor()->GetDefaultConfiguration(
        base::BindOnce(&TestSensorClient::OnGetDefaultConfiguration,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }
}

// Tests adding a valid configuration. Client should be notified by
// SensorClient::SensorReadingChanged().
TEST_F(GenericSensorServiceTest, ValidAddConfigurationTest) {
  auto client = std::make_unique<TestSensorClient>(SensorType::AMBIENT_LIGHT);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::AMBIENT_LIGHT,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  PlatformSensorConfiguration configuration(50.0);
  client->sensor()->AddConfiguration(
      configuration,
      base::BindOnce(&TestSensorClient::OnAddConfiguration,
                     base::Unretained(client.get()),
                     base::BindOnce(&CheckSuccess, base::DoNothing(), true)));

  {
    // Expect the SensorReadingChanged() will be called after AddConfiguration.
    base::RunLoop run_loop;
    client->SetCheckValueCallback(base::BindOnce(&CheckValue, 50.0));
    client->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }
}

// Tests adding an invalid configuation, the max allowed frequency is 50.0 in
// the mocked SensorImpl, while we add one with 60.0.
TEST_F(GenericSensorServiceTest, InvalidAddConfigurationTest) {
  auto client =
      std::make_unique<TestSensorClient>(SensorType::LINEAR_ACCELERATION);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::LINEAR_ACCELERATION,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  {
    // Invalid configuration that exceeds the max allowed frequency.
    base::RunLoop run_loop;
    PlatformSensorConfiguration configuration(60.0);
    client->sensor()->AddConfiguration(
        configuration,
        base::BindOnce(
            &TestSensorClient::OnAddConfiguration,
            base::Unretained(client.get()),
            base::BindOnce(&CheckSuccess, run_loop.QuitClosure(), false)));
    run_loop.Run();
  }
}

// Tests adding more than one clients. Sensor should send notification to all
// its clients.
TEST_F(GenericSensorServiceTest, MultipleClientsTest) {
  auto client_1 = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  auto client_2 = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_1.get()), barrier_closure));
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_2.get()), barrier_closure));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    PlatformSensorConfiguration configuration(48.0);
    client_1->sensor()->AddConfiguration(
        configuration,
        base::BindOnce(
            &TestSensorClient::OnAddConfiguration,
            base::Unretained(client_1.get()),
            base::BindOnce(&CheckSuccess, run_loop.QuitClosure(), true)));
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called for both clients.
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    client_1->SetCheckValueCallback(base::BindOnce(&CheckValue, 48.0));
    client_2->SetCheckValueCallback(base::BindOnce(&CheckValue, 48.0));
    client_1->SetQuitClosure(barrier_closure);
    client_2->SetQuitClosure(barrier_closure);
    run_loop.Run();
  }
}

// Tests adding more than one clients. If mojo connection is broken on one
// client, other clients should not be affected.
TEST_F(GenericSensorServiceTest, ClientMojoConnectionBrokenTest) {
  auto client_1 = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  auto client_2 = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_1.get()), barrier_closure));
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_2.get()), barrier_closure));
    run_loop.Run();
  }

  // Breaks mojo connection of client_1.
  client_1->ResetSensor();

  {
    base::RunLoop run_loop;
    PlatformSensorConfiguration configuration(48.0);
    client_2->sensor()->AddConfiguration(
        configuration,
        base::BindOnce(
            &TestSensorClient::OnAddConfiguration,
            base::Unretained(client_2.get()),
            base::BindOnce(&CheckSuccess, run_loop.QuitClosure(), true)));
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called on client_2.
  {
    base::RunLoop run_loop;
    client_2->SetCheckValueCallback(base::BindOnce(&CheckValue, 48.0));
    client_2->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }
}

// Test add and remove configuration operations.
TEST_F(GenericSensorServiceTest, AddAndRemoveConfigurationTest) {
  auto client = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called. The frequency value
  // should be 10.0.
  {
    PlatformSensorConfiguration configuration_10(10.0);
    client->sensor()->AddConfiguration(
        configuration_10,
        base::BindOnce(&TestSensorClient::OnAddConfiguration,
                       base::Unretained(client.get()),
                       base::BindOnce(&CheckSuccess, base::DoNothing(), true)));
    base::RunLoop run_loop;
    client->SetCheckValueCallback(base::BindOnce(&CheckValue, 10.0));
    client->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called. The frequency value
  // should be 40.0.
  PlatformSensorConfiguration configuration_40(40.0);
  client->sensor()->AddConfiguration(
      configuration_40,
      base::BindOnce(&TestSensorClient::OnAddConfiguration,
                     base::Unretained(client.get()),
                     base::BindOnce(&CheckSuccess, base::DoNothing(), true)));
  {
    base::RunLoop run_loop;
    client->SetCheckValueCallback(base::BindOnce(&CheckValue, 40.0));
    client->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  // After |configuration_40| is removed, expect the SensorReadingChanged() will
  // be called. The frequency value should be 10.0.
  {
    base::RunLoop run_loop;
    client->sensor()->RemoveConfiguration(configuration_40);
    client->SetCheckValueCallback(base::BindOnce(&CheckValue, 10.0));
    client->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }
}

// Test suspend. After suspending, the client won't be notified by
// SensorReadingChanged() after calling AddConfiguration.
// Call the AddConfiguration() twice, if the SensorReadingChanged() was
// called, it should happen before the callback triggerred by the second
// AddConfiguration(). In this way we make sure it won't be missed by the
// early quit of main thread (when there is an unexpected notification by
// SensorReadingChanged()).
TEST_F(GenericSensorServiceTest, SuspendTest) {
  auto client = std::make_unique<TestSensorClient>(SensorType::AMBIENT_LIGHT);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::AMBIENT_LIGHT,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  client->sensor()->Suspend();

  // Expect the SensorReadingChanged() won't be called. Pass a bad value(123.0)
  // to |check_value_| to guarantee SensorReadingChanged() really doesn't be
  // called. Otherwise the CheckValue() will complain on the bad value.
  client->SetCheckValueCallback(base::BindOnce(&CheckValue, 123.0));

  base::RunLoop run_loop;
  PlatformSensorConfiguration configuration_1(30.0);
  client->sensor()->AddConfiguration(
      configuration_1,
      base::BindOnce(&TestSensorClient::OnAddConfiguration,
                     base::Unretained(client.get()),
                     base::BindOnce(&CheckSuccess, base::DoNothing(), true)));
  PlatformSensorConfiguration configuration_2(31.0);
  client->sensor()->AddConfiguration(
      configuration_2,
      base::BindOnce(
          &TestSensorClient::OnAddConfiguration, base::Unretained(client.get()),
          base::BindOnce(&CheckSuccess, run_loop.QuitClosure(), true)));
  run_loop.Run();
}

// Test suspend and resume. After resuming, client can add configuration and
// be notified by SensorReadingChanged() as usual.
TEST_F(GenericSensorServiceTest, SuspendThenResumeTest) {
  auto client = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  {
    base::RunLoop run_loop;
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client.get()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Expect the SensorReadingChanged() will be called. The frequency should
  // be 10.0 after AddConfiguration.
  {
    base::RunLoop run_loop;
    PlatformSensorConfiguration configuration_1(10.0);
    client->sensor()->AddConfiguration(
        configuration_1,
        base::BindOnce(&TestSensorClient::OnAddConfiguration,
                       base::Unretained(client.get()),
                       base::BindOnce(&CheckSuccess, base::DoNothing(), true)));
    client->SetCheckValueCallback(base::BindOnce(&CheckValue, 10.0));
    client->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  client->sensor()->Suspend();
  client->sensor()->Resume();

  // Expect the SensorReadingChanged() will be called. The frequency should
  // be 50.0 after new configuration is added.
  {
    base::RunLoop run_loop;
    PlatformSensorConfiguration configuration_2(50.0);
    client->sensor()->AddConfiguration(
        configuration_2,
        base::BindOnce(&TestSensorClient::OnAddConfiguration,
                       base::Unretained(client.get()),
                       base::BindOnce(&CheckSuccess, base::DoNothing(), true)));
    client->SetCheckValueCallback(base::BindOnce(&CheckValue, 50.0));
    client->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }
}

// Test suspend when there are more than one client. The suspended client won't
// receive SensorReadingChanged() notification.
TEST_F(GenericSensorServiceTest, MultipleClientsSuspendAndResumeTest) {
  auto client_1 = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  auto client_2 = std::make_unique<TestSensorClient>(SensorType::PRESSURE);
  {
    base::RunLoop run_loop;
    auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_1.get()), barrier_closure));
    sensor_provider_->GetSensor(
        SensorType::PRESSURE,
        base::BindOnce(&TestSensorClient::OnSensorCreated,
                       base::Unretained(client_2.get()), barrier_closure));
    run_loop.Run();
  }

  client_1->sensor()->Suspend();

  {
    base::RunLoop run_loop;
    PlatformSensorConfiguration configuration(46.0);
    client_2->sensor()->AddConfiguration(
        configuration,
        base::BindOnce(
            &TestSensorClient::OnAddConfiguration,
            base::Unretained(client_2.get()),
            base::BindOnce(&CheckSuccess, run_loop.QuitClosure(), true)));
    run_loop.Run();
  }

  // Expect the sensor_2 will receive SensorReadingChanged() notification while
  // sensor_1 won't.
  {
    base::RunLoop run_loop;
    client_2->SetCheckValueCallback(base::BindOnce(&CheckValue, 46.0));
    client_2->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }
}

}  //  namespace

}  //  namespace device
