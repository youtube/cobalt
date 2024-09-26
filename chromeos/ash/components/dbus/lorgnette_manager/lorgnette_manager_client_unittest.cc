// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
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
using ::testing::WithArgs;

namespace ash {

namespace {

// Fake scanner name used in the tests.
constexpr char kScannerDeviceName[] = "test:MX3100_192.168.0.3";
// Fake scan UUIDs used in the tests.
constexpr char kScanUuid[] = "uuid";
constexpr char kSecondScanUuid[] = "second uuid";
// Fake progress reported by the tests.
constexpr uint32_t kProgress = 51;
// Fake page numbers reported by the tests.
constexpr uint32_t kFirstPageNum = 10;
constexpr uint32_t kSecondPageNum = 11;
// Fake scan data reported by the tests.
constexpr char kFirstScanData[] = "Hello!";
constexpr char kSecondScanData[] = "Goodbye!";

// Convenience method for creating a lorgnette::ListScannersResponse.
lorgnette::ListScannersResponse CreateListScannersResponse() {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("Name");
  scanner.set_manufacturer("Manufacturer");
  scanner.set_model("Model");
  scanner.set_type("Type");

  lorgnette::ListScannersResponse response;
  *response.add_scanners() = std::move(scanner);

  return response;
}

// Convenience method for creating a lorgnette::ScannerCapabilities.
lorgnette::ScannerCapabilities CreateScannerCapabilities() {
  lorgnette::ScannableArea area;
  area.set_width(8.5);
  area.set_height(11.0);

  lorgnette::DocumentSource source;
  source.set_type(lorgnette::SourceType::SOURCE_ADF_SIMPLEX);
  source.set_name("Source name");
  *source.mutable_area() = std::move(area);

  lorgnette::ScannerCapabilities capabilities;
  capabilities.add_resolutions(100);
  *capabilities.add_sources() = std::move(source);
  capabilities.add_color_modes(lorgnette::ColorMode::MODE_COLOR);

  return capabilities;
}

// Convenience method for creating a lorgnette::StartScanRequest.
lorgnette::StartScanRequest CreateStartScanRequest() {
  lorgnette::ScanRegion region;
  region.set_top_left_x(10);
  region.set_top_left_y(12);
  region.set_bottom_right_x(90);
  region.set_bottom_right_y(80);

  lorgnette::ScanSettings settings;
  settings.set_resolution(1080);
  settings.set_color_mode(lorgnette::ColorMode::MODE_GRAYSCALE);
  settings.set_source_name("Source name, round 2");
  *settings.mutable_scan_region() = std::move(region);

  lorgnette::StartScanRequest request;
  request.set_device_name(kScannerDeviceName);
  *request.mutable_settings() = std::move(settings);

  return request;
}

// Convenience method for creating a dbus::Response containing a
// lorgnette::StartScanResponse with the given |state|. If
// |state| == lorgnette::ScanState::SCAN_STATE_FAILED, this method will add an
// appropriate failure reason. Only specify |scan_uuid| if multiple scans are
// necessary.
std::unique_ptr<dbus::Response> CreateStartScanResponse(
    lorgnette::ScanState state,
    const std::string& scan_uuid = kScanUuid,
    const lorgnette::ScanFailureMode failure_mode =
        lorgnette::SCAN_FAILURE_MODE_NO_FAILURE) {
  lorgnette::StartScanResponse response;
  response.set_state(state);
  response.set_scan_uuid(scan_uuid);

  if (state == lorgnette::ScanState::SCAN_STATE_FAILED) {
    response.set_failure_reason(
        "The small elf inside the scanner is feeling overworked.");
    response.set_scan_failure_mode(failure_mode);
  }

  std::unique_ptr<dbus::Response> start_scan_response =
      dbus::Response::CreateEmpty();
  EXPECT_TRUE(dbus::MessageWriter(start_scan_response.get())
                  .AppendProtoAsArrayOfBytes(response));

  return start_scan_response;
}

// Convenience method for creating a lorgnette::GetNextImageRequest. Only
// specify |scan_uuid| if multiple scans are necessary.
lorgnette::GetNextImageRequest CreateGetNextImageRequest(
    const std::string& scan_uuid = kScanUuid) {
  lorgnette::GetNextImageRequest request;
  request.set_scan_uuid(scan_uuid);
  return request;
}

// Convenience method for creating a dbus::Response containing a
// lorgnette::GetNextImageResponse. If |success| is false, this method will add
// an appropriate failure reason.
std::unique_ptr<dbus::Response> CreateGetNextImageResponse(
    bool success,
    const lorgnette::ScanFailureMode failure_mode =
        lorgnette::SCAN_FAILURE_MODE_NO_FAILURE) {
  lorgnette::GetNextImageResponse response;
  response.set_success(success);

  if (!success) {
    response.set_failure_reason("PC LOAD LETTER");
    response.set_scan_failure_mode(failure_mode);
  }

  std::unique_ptr<dbus::Response> get_next_image_response =
      dbus::Response::CreateEmpty();
  EXPECT_TRUE(dbus::MessageWriter(get_next_image_response.get())
                  .AppendProtoAsArrayOfBytes(response));

  return get_next_image_response;
}

// Convenience method for creating a lorgnette::CancelScanRequest.
lorgnette::CancelScanRequest CreateCancelScanRequest() {
  lorgnette::CancelScanRequest request;
  request.set_scan_uuid(kScanUuid);
  return request;
}

// Convenience method for creating a dbus::Response containing a
// lorgnette::CancelScanResponse.
std::unique_ptr<dbus::Response> CreateCancelScanResponse(bool success) {
  lorgnette::CancelScanResponse response;
  response.set_success(success);

  std::unique_ptr<dbus::Response> cancel_scan_response =
      dbus::Response::CreateEmpty();
  EXPECT_TRUE(dbus::MessageWriter(cancel_scan_response.get())
                  .AppendProtoAsArrayOfBytes(response));

  return cancel_scan_response;
}

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

// Matcher that veries two protobufs contain the same data.
MATCHER_P(ProtobufEquals, expected_message, "") {
  std::string arg_dumped;
  arg.SerializeToString(&arg_dumped);
  std::string expected_message_dumped;
  expected_message.SerializeToString(&expected_message_dumped);
  return arg_dumped == expected_message_dumped;
}

}  // namespace

class LorgnetteManagerClientTest : public testing::Test {
 public:
  LorgnetteManagerClientTest() = default;
  LorgnetteManagerClientTest(const LorgnetteManagerClientTest&) = delete;
  LorgnetteManagerClientTest& operator=(const LorgnetteManagerClientTest&) =
      delete;

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock lorgnette daemon proxy.
    mock_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), lorgnette::kManagerServiceName,
        dbus::ObjectPath(lorgnette::kManagerServicePath));

    // The client's Init() method should request a proxy for communicating with
    // the lorgnette daemon.
    EXPECT_CALL(
        *mock_bus_.get(),
        GetObjectProxy(lorgnette::kManagerServiceName,
                       dbus::ObjectPath(lorgnette::kManagerServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    // Save the client's scan status changed signal callback.
    EXPECT_CALL(*mock_proxy_.get(),
                DoConnectToSignal(lorgnette::kManagerServiceInterface,
                                  lorgnette::kScanStatusChangedSignal, _, _))
        .WillOnce(WithArgs<2, 3>(Invoke(
            [&](dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
              scan_status_changed_signal_callback_ = std::move(signal_callback);

              task_environment_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                            lorgnette::kManagerServiceInterface,
                                            lorgnette::kScanStatusChangedSignal,
                                            /*success=*/true));
            })));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create and initialize a client with the mock bus.
    LorgnetteManagerClient::Initialize(mock_bus_.get());

    // Execute callbacks posted by Init().
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    LorgnetteManagerClient::Shutdown();
    mock_bus_->ShutdownAndBlock();
  }

  // A shorter name to return the LorgnetteManagerClient under test.
  static LorgnetteManagerClient* GetClient() {
    return LorgnetteManagerClient::Get();
  }

  // Adds an expectation to |mock_proxy_| that kListScannersMethod will be
  // called. When called, |mock_proxy_| will respond with |response|.
  void SetListScannersExpectation(dbus::Response* response) {
    list_scanners_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kListScannersMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(
            Invoke(this, &LorgnetteManagerClientTest::OnCallListScanners));
  }

  // Adds an expectation to |mock_proxy_| that kGetScannerCapabilitiesMethod
  // will be called. When called, |mock_proxy_| will respond with |response|.
  void SetGetScannerCapabilitiesExpectation(dbus::Response* response) {
    get_scanner_capabilities_response_ = response;
    EXPECT_CALL(
        *mock_proxy_.get(),
        DoCallMethod(HasMember(lorgnette::kGetScannerCapabilitiesMethod),
                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(
            this, &LorgnetteManagerClientTest::OnCallGetScannerCapabilities));
  }

  // Adds an expectation to |mock_proxy_| that kStartScanMethod will be called.
  // When called, |mock_proxy_| will respond with |response|.
  void SetStartScanExpectation(dbus::Response* response) {
    start_scan_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kStartScanMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(this, &LorgnetteManagerClientTest::OnCallStartScan));
  }

  // Adds an expectation to |mock_proxy_| that kGetNextImageMethod will be
  // called. Adds |response| to the end of a FIFO queue of responses used by
  // |mock_proxy_|. Only specify |scan_uuid| if multiple scans are
  // necessary.
  void SetGetNextImageExpectation(dbus::Response* response,
                                  const std::string& scan_uuid = kScanUuid) {
    get_next_image_responses_.push({response, scan_uuid});
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kGetNextImageMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillRepeatedly(
            Invoke(this, &LorgnetteManagerClientTest::OnCallGetNextImage));
  }

  // Adds an expectation to |mock_proxy_| that kCancelScanMethod will be called.
  // When called, |mock_proxy_| will respond with |response|.
  void SetCancelScanExpectation(dbus::Response* response) {
    cancel_scan_response_ = response;
    EXPECT_CALL(*mock_proxy_.get(),
                DoCallMethod(HasMember(lorgnette::kCancelScanMethod),
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
        .WillOnce(Invoke(this, &LorgnetteManagerClientTest::OnCallCancelScan));
  }

  // Tells |mock_proxy_| to emit a kScanStatusChangedSignal with the given
  // |uuid|, |state|, |page|, |progress|, |more_pages|, and |failure_mode|.
  void EmitScanStatusChangedSignal(
      const std::string& uuid,
      const lorgnette::ScanState state,
      const int page,
      const int progress,
      const bool more_pages,
      const lorgnette::ScanFailureMode failure_mode =
          lorgnette::SCAN_FAILURE_MODE_NO_FAILURE) {
    lorgnette::ScanStatusChangedSignal proto;
    proto.set_scan_uuid(uuid);
    proto.set_state(state);
    proto.set_page(page);
    proto.set_progress(progress);
    proto.set_more_pages(more_pages);
    proto.set_scan_failure_mode(failure_mode);

    dbus::Signal signal(lorgnette::kManagerServiceInterface,
                        lorgnette::kScanStatusChangedSignal);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);

    scan_status_changed_signal_callback_.Run(&signal);

    base::RunLoop().RunUntilIdle();
  }

  // Tells |mock_proxy_| to emit a kScanStatusChangedSignal with no data. This
  // is useful for testing failure cases.
  void EmitEmptyScanStatusChangedSignal() {
    dbus::Signal signal(lorgnette::kManagerServiceInterface,
                        lorgnette::kScanStatusChangedSignal);
    scan_status_changed_signal_callback_.Run(&signal);
  }

  // Writes |data| to the ongoing scan job's file descriptor. This method should
  // only be called when a scan job exists.
  void WriteDataToScanJob(const std::string& data) {
    ASSERT_TRUE(fd_.is_valid());
    EXPECT_TRUE(base::WriteFileDescriptor(fd_.get(), data));
    fd_.reset();
    task_environment_.RunUntilIdle();
  }

 private:
  // Responsible for responding to a kListScannersMethod call.
  void OnCallListScanners(dbus::MethodCall* method_call,
                          int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), list_scanners_response_));
  }

  // Responsible for responding to a kGetScannerCapabilitiesMethod call, and
  // verifying that |method_call| was formatted correctly.
  void OnCallGetScannerCapabilities(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the scanner name was sent correctly.
    std::string device_name;
    ASSERT_TRUE(dbus::MessageReader(method_call).PopString(&device_name));
    EXPECT_EQ(device_name, kScannerDeviceName);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback),
                                  get_scanner_capabilities_response_));
  }

  // Responsible for responding to a kStartScanMethod call and verifying that
  // |method_call| was formatted correctly.
  void OnCallStartScan(dbus::MethodCall* method_call,
                       int timeout_ms,
                       dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the start scan request was created and sent correctly.
    lorgnette::StartScanRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request, ProtobufEquals(CreateStartScanRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), start_scan_response_));
  }

  // Responsible for responding to a kGetNextImageMethod call and verifying that
  // |method_call| was formatted correctly.
  void OnCallGetNextImage(dbus::MethodCall* method_call,
                          int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the get next image request was sent correctly.
    ASSERT_FALSE(get_next_image_responses_.empty());
    auto response_and_uuid = get_next_image_responses_.front();
    dbus::MessageReader message_reader(method_call);
    lorgnette::GetNextImageRequest request;
    ASSERT_TRUE(message_reader.PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(
        request,
        ProtobufEquals(CreateGetNextImageRequest(response_and_uuid.second)));
    // Save the file descriptor so we can write to it later.
    EXPECT_TRUE(message_reader.PopFileDescriptor(&fd_));

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), response_and_uuid.first));
    get_next_image_responses_.pop();
  }

  // Responsible for responding to a kCancelScanMethod call and verifying that
  // |method_call| was formatted correctly.
  void OnCallCancelScan(dbus::MethodCall* method_call,
                        int timeout_ms,
                        dbus::ObjectProxy::ResponseCallback* callback) {
    // Verify that the cancel scan request was created and sent correctly.
    lorgnette::CancelScanRequest request;
    ASSERT_TRUE(
        dbus::MessageReader(method_call).PopArrayOfBytesAsProto(&request));
    EXPECT_THAT(request, ProtobufEquals(CreateCancelScanRequest()));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), cancel_scan_response_));
  }

  // A message loop to emulate asynchronous behavior.
  base::test::TaskEnvironment task_environment_;
  // Mock D-Bus objects for the client to interact with.
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // Holds the client's kScanStatusChangedSignal callback.
  dbus::ObjectProxy::SignalCallback scan_status_changed_signal_callback_;
  // Used to respond to kListScannersMethod D-Bus calls.
  raw_ptr<dbus::Response, ExperimentalAsh> list_scanners_response_ = nullptr;
  // Used to respond to kGetScannerCapabilitiesMethod D-Bus calls.
  raw_ptr<dbus::Response, ExperimentalAsh> get_scanner_capabilities_response_ =
      nullptr;
  // Used to respond to kStartScanMethod D-Bus calls.
  raw_ptr<dbus::Response, ExperimentalAsh> start_scan_response_ = nullptr;
  // Used to respond to kCancelScanMethod D-Bus calls.
  raw_ptr<dbus::Response, ExperimentalAsh> cancel_scan_response_ = nullptr;
  // Used to respond to kGetNextImageMethod D-Bus calls. A single call to some
  // of LorgnetteManagerClient's methods can result in multiple
  // kGetNextImageMethod D-Bus calls, so we need to queue the responses. Also
  // records the scan_uuid used to make the kGetNextImageMethod call.
  base::queue<std::pair<dbus::Response*, std::string>>
      get_next_image_responses_;
  // Used to write data to ongoing scan jobs.
  base::ScopedFD fd_;
};

// Test that the client can retrieve a list of scanners.
TEST_F(LorgnetteManagerClientTest, ListScanners) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const lorgnette::ListScannersResponse kExpectedResponse =
      CreateListScannersResponse();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetListScannersExpectation(response.get());

  base::RunLoop run_loop;
  GetClient()->ListScanners(base::BindLambdaForTesting(
      [&](absl::optional<lorgnette::ListScannersResponse> result) {
        ASSERT_TRUE(result.has_value());
        EXPECT_THAT(result.value(), ProtobufEquals(kExpectedResponse));
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that the client handles a null response to a kListScannersMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToListScanners) {
  SetListScannersExpectation(nullptr);

  base::RunLoop run_loop;
  GetClient()->ListScanners(base::BindLambdaForTesting(
      [&](absl::optional<lorgnette::ListScannersResponse> result) {
        EXPECT_EQ(result, absl::nullopt);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that the client handles a response to a kListScannersMethod D-Bus call
// without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToListScanners) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetListScannersExpectation(response.get());

  base::RunLoop run_loop;
  GetClient()->ListScanners(base::BindLambdaForTesting(
      [&](absl::optional<lorgnette::ListScannersResponse> result) {
        EXPECT_EQ(result, absl::nullopt);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Test that the client can retrieve capabilities for a scanner.
TEST_F(LorgnetteManagerClientTest, GetScannerCapabilities) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  const lorgnette::ScannerCapabilities kExpectedResponse =
      CreateScannerCapabilities();
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(kExpectedResponse));
  SetGetScannerCapabilitiesExpectation(response.get());

  base::RunLoop run_loop;
  GetClient()->GetScannerCapabilities(
      kScannerDeviceName,
      base::BindLambdaForTesting(
          [&](absl::optional<lorgnette::ScannerCapabilities> result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_THAT(result.value(), ProtobufEquals(kExpectedResponse));
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a null response to a
// kGetScannerCapabilitiesMethod D-Bus call.
TEST_F(LorgnetteManagerClientTest, NullResponseToGetScannerCapabilities) {
  SetGetScannerCapabilitiesExpectation(nullptr);

  base::RunLoop run_loop;
  GetClient()->GetScannerCapabilities(
      kScannerDeviceName,
      base::BindLambdaForTesting(
          [&](absl::optional<lorgnette::ScannerCapabilities> result) {
            EXPECT_EQ(result, absl::nullopt);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles a response to a kGetScannerCapabilitiesMethod
// D-Bus call without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToGetScannerCapabilities) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetGetScannerCapabilitiesExpectation(response.get());

  base::RunLoop run_loop;
  GetClient()->GetScannerCapabilities(
      kScannerDeviceName,
      base::BindLambdaForTesting(
          [&](absl::optional<lorgnette::ScannerCapabilities> result) {
            EXPECT_EQ(result, absl::nullopt);
            run_loop.Quit();
          }));

  run_loop.Run();
}

// Test that the client handles progress signals.
TEST_F(LorgnetteManagerClientTest, ProgressSignal) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(), base::NullCallback(),
      base::NullCallback(),
      base::BindLambdaForTesting([&](uint32_t progress, uint32_t page_num) {
        EXPECT_EQ(progress, kProgress);
        EXPECT_EQ(page_num, kFirstPageNum);
        run_loop.Quit();
      }));

  base::RunLoop().RunUntilIdle();

  EmitScanStatusChangedSignal(kScanUuid,
                              lorgnette::ScanState::SCAN_STATE_IN_PROGRESS,
                              kFirstPageNum, kProgress, /*more_pages=*/true);

  run_loop.Run();
}

// Test that the client handles data written to a scan job before receiving a
// SCAN_STATE_PAGE_COMPLETE signal.
TEST_F(LorgnetteManagerClientTest, ScanDataWrittenBeforeSignalReceived) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop completion_run_loop;
  base::RunLoop page_run_loop;
  uint8_t num_pages_scanned = 0;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
        EXPECT_EQ(num_pages_scanned, 1);
        completion_run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](std::string data, uint32_t page_num) {
        EXPECT_EQ(page_num, kFirstPageNum);
        EXPECT_EQ(data, kFirstScanData);
        num_pages_scanned++;
        page_run_loop.Quit();
      }),
      base::NullCallback());

  base::RunLoop().RunUntilIdle();

  WriteDataToScanJob(kFirstScanData);

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED, kFirstPageNum,
      /*progress=*/100, /*more_pages=*/false);

  page_run_loop.Run();

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_COMPLETED, kFirstPageNum,
      /*progress=*/100, /*more_pages=*/false);

  completion_run_loop.Run();
}

// Test that the client handles SCAN_STATE_PAGE_COMPLETE signals received before
// data is written to a scan job.
TEST_F(LorgnetteManagerClientTest, SignalReceivedBeforeScanDataWritten) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop completion_run_loop;
  base::RunLoop page_run_loop;
  uint8_t num_pages_scanned = 0;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
        EXPECT_EQ(num_pages_scanned, 1);
        completion_run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](std::string data, uint32_t page_num) {
        EXPECT_EQ(page_num, kFirstPageNum);
        EXPECT_EQ(data, kFirstScanData);
        num_pages_scanned++;
        page_run_loop.Quit();
      }),
      base::NullCallback());

  base::RunLoop().RunUntilIdle();

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED, kFirstPageNum,
      /*progress=*/100, /*more_pages=*/false);

  WriteDataToScanJob(kFirstScanData);

  page_run_loop.Run();

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_COMPLETED, kFirstPageNum,
      /*progress=*/100, /*more_pages=*/false);

  completion_run_loop.Run();
}

// Test that the client handles a multi-page scan.
TEST_F(LorgnetteManagerClientTest, MultiPageScan) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  auto get_next_image_response = CreateGetNextImageResponse(true);
  // Since we have two pages, GetNextImage() will be called twice.
  SetGetNextImageExpectation(get_next_image_response.get());
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop completion_run_loop;
  base::RunLoop first_page_run_loop;
  base::RunLoop second_page_run_loop;
  uint8_t num_pages_scanned = 0;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
        EXPECT_EQ(num_pages_scanned, 2);
        completion_run_loop.Quit();
      }),
      base::BindLambdaForTesting([&](std::string data, uint32_t page_num) {
        if (page_num == kFirstPageNum) {
          EXPECT_EQ(data, kFirstScanData);
          first_page_run_loop.Quit();
        } else if (page_num == kSecondPageNum) {
          EXPECT_EQ(data, kSecondScanData);
          second_page_run_loop.Quit();
        }
        num_pages_scanned++;
      }),
      base::NullCallback());

  base::RunLoop().RunUntilIdle();

  WriteDataToScanJob(kFirstScanData);

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED, kFirstPageNum,
      /*progress=*/100, /*more_pages=*/true);

  first_page_run_loop.Run();

  WriteDataToScanJob(kSecondScanData);

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED,
      kSecondPageNum, /*progress=*/100, /*more_pages=*/false);

  second_page_run_loop.Run();

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_COMPLETED, kSecondPageNum,
      /*progress=*/100, /*more_pages=*/false);

  completion_run_loop.Run();
}

// Test that the client handles a kScanStatusChangedSignal with state
// SCAN_STATE_FAILED.
TEST_F(LorgnetteManagerClientTest, ScanStateFailedSignal) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  base::RunLoop().RunUntilIdle();

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_FAILED, kFirstPageNum,
      /*progress=*/100, /*more_pages=*/false,
      lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY);

  run_loop.Run();
}

// Test that the client handles a null response to a kStartScanMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToStartScan) {
  SetStartScanExpectation(nullptr);

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kStartScanMethod D-Bus call
// without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToStartScan) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  SetStartScanExpectation(response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kStartScanMethod D-Bus call
// with state lorgnette::SCAN_STATE_FAILED.
TEST_F(LorgnetteManagerClientTest, StartScanScanStateFailed) {
  auto response = CreateStartScanResponse(
      lorgnette::ScanState::SCAN_STATE_FAILED, kScanUuid,
      lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED);
  SetStartScanExpectation(response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a null response to a kGetNextImageMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToGetNextImage) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  SetGetNextImageExpectation(nullptr);

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kGetNextImageMethod D-Bus call
// without a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToGetNextImage) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  std::unique_ptr<dbus::Response> get_next_image_response =
      dbus::Response::CreateEmpty();
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles a response to a kGetNextImageMethod D-Bus call
// with success equal to false.
TEST_F(LorgnetteManagerClientTest, GetNextImageScanStateFailed) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  auto get_next_image_response =
      CreateGetNextImageResponse(false, lorgnette::SCAN_FAILURE_MODE_IO_ERROR);
  SetGetNextImageExpectation(get_next_image_response.get());

  base::RunLoop run_loop;
  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(
      request.device_name(), request.settings(),
      base::BindLambdaForTesting([&](lorgnette::ScanFailureMode failure_mode) {
        EXPECT_EQ(failure_mode, lorgnette::SCAN_FAILURE_MODE_IO_ERROR);
        run_loop.Quit();
      }),
      base::NullCallback(), base::NullCallback());

  run_loop.Run();
}

// Test that the client handles an unexpected kScanStatusChangedSignal.
TEST_F(LorgnetteManagerClientTest, UnexpectedScanStatusChangedSignal) {
  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_PAGE_COMPLETED, /*page=*/1,
      /*progress=*/100,
      /*more_pages=*/true);
}

// Test that the client handles a kScanStatusChangedSignal without a valid
// proto.
TEST_F(LorgnetteManagerClientTest, EmptyScanStatusChangedSignal) {
  EmitEmptyScanStatusChangedSignal();
}

// Test that the client can cancel an existing scan job.
TEST_F(LorgnetteManagerClientTest, CancelScanJob) {
  const auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  const auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());
  const auto cancel_scan_response = CreateCancelScanResponse(true);
  SetCancelScanExpectation(cancel_scan_response.get());

  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(request.device_name(), request.settings(),
                         base::NullCallback(), base::NullCallback(),
                         base::NullCallback());

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  GetClient()->CancelScan(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    run_loop.Quit();
  }));

  base::RunLoop().RunUntilIdle();

  EmitScanStatusChangedSignal(
      kScanUuid, lorgnette::ScanState::SCAN_STATE_CANCELLED, kFirstPageNum,
      /*progress=*/67, /*more_pages=*/false);

  run_loop.Run();
}

// Test that the client handles a cancel call with no existing scan jobs.
TEST_F(LorgnetteManagerClientTest, CancelScanJobNoExistingJobs) {
  base::RunLoop run_loop;
  GetClient()->CancelScan(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    run_loop.Quit();
  }));

  run_loop.Run();
}

// Test that the client handles a cancel call when multiple scan jobs exist.
TEST_F(LorgnetteManagerClientTest, CancelScanJobMultipleJobsExist) {
  auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  const auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());

  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(request.device_name(), request.settings(),
                         base::NullCallback(), base::NullCallback(),
                         base::NullCallback());

  base::RunLoop().RunUntilIdle();

  start_scan_response = CreateStartScanResponse(
      lorgnette::ScanState::SCAN_STATE_IN_PROGRESS, kSecondScanUuid);
  SetStartScanExpectation(start_scan_response.get());
  SetGetNextImageExpectation(get_next_image_response.get(), kSecondScanUuid);
  GetClient()->StartScan(request.device_name(), request.settings(),
                         base::NullCallback(), base::NullCallback(),
                         base::NullCallback());

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  GetClient()->CancelScan(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    run_loop.Quit();
  }));

  run_loop.Run();
}

// Test that the client handles a null response to a kCancelScanMethod D-Bus
// call.
TEST_F(LorgnetteManagerClientTest, NullResponseToCancelScanJob) {
  const auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  const auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());
  SetCancelScanExpectation(nullptr);

  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(request.device_name(), request.settings(),
                         base::NullCallback(), base::NullCallback(),
                         base::NullCallback());

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  GetClient()->CancelScan(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    run_loop.Quit();
  }));

  run_loop.Run();
}

// Test that the client handles a response to a kCancelScanMethod D-Bus call
// which doesn't have a valid proto.
TEST_F(LorgnetteManagerClientTest, EmptyResponseToCancelScanJob) {
  const auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  const auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());
  const auto cancel_scan_response = dbus::Response::CreateEmpty();
  SetCancelScanExpectation(cancel_scan_response.get());

  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(request.device_name(), request.settings(),
                         base::NullCallback(), base::NullCallback(),
                         base::NullCallback());

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  GetClient()->CancelScan(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    run_loop.Quit();
  }));

  run_loop.Run();
}

// Test that the client handles a kCancelScanMethod D-Bus call failing.
TEST_F(LorgnetteManagerClientTest, CancelScanJobCallFails) {
  const auto start_scan_response =
      CreateStartScanResponse(lorgnette::ScanState::SCAN_STATE_IN_PROGRESS);
  SetStartScanExpectation(start_scan_response.get());
  const auto get_next_image_response = CreateGetNextImageResponse(true);
  SetGetNextImageExpectation(get_next_image_response.get());
  const auto cancel_scan_response = CreateCancelScanResponse(false);
  SetCancelScanExpectation(cancel_scan_response.get());

  lorgnette::StartScanRequest request = CreateStartScanRequest();
  GetClient()->StartScan(request.device_name(), request.settings(),
                         base::NullCallback(), base::NullCallback(),
                         base::NullCallback());

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  GetClient()->CancelScan(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    run_loop.Quit();
  }));

  run_loop.Run();
}

}  // namespace ash
