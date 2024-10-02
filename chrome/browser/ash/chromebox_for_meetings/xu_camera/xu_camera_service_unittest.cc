// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"

#include <asm-generic/errno.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <cstdint>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "xu_camera_service.h"

namespace ash::cfm {
namespace {

constexpr char kFakePath[] = "/fake/dev/path";
const std::vector<uint8_t> kGuid = {0x12, 0x34, 0x12, 0x34, 0x12, 0x34,
                                    0x12, 0x34, 0x12, 0x34, 0x12, 0x34,
                                    0x12, 0x34, 0x12, 0x34};
const auto kMenuEntries = mojom::MenuEntries::New();
const std::vector<uint8_t> kName(32, 'a');
const std::vector<uint8_t> kData = {0x43, 0x21};
const std::vector<uint8_t> kLen = {0x02, 0x00};  // little-endian uint16
const int32_t kValue = 123;                      // Fake v4l2 value
const std::vector<uint8_t> kValueAsUint8 = {
    (std::uint8_t*)&(kValue), (std::uint8_t*)&(kValue) + sizeof(std::int32_t)};

class TestDelegate : public XuCameraService::Delegate {
 public:
  int Ioctl(const base::ScopedFD& fd,
            unsigned int request,
            void* query) override {
    if (VIDIOC_G_CTRL == request) {
      struct v4l2_control* control = static_cast<v4l2_control*>(query);
      control->value = kValue;
    } else if (UVCIOC_CTRL_QUERY == request) {
      uvc_xu_control_query* control_query =
          static_cast<uvc_xu_control_query*>(query);
      if (UVC_GET_LEN == control_query->query) {
        control_query->data[0] = kLen[0];
        control_query->data[1] = kLen[1];
      } else if (UVC_GET_CUR == control_query->query) {
        control_query->data[0] = kData[0];
        control_query->data[1] = kData[1];
      }
    }
    return 0;
  }

  bool OpenFile(base::ScopedFD& fd, const std::string& path) override {
    if (path.empty()) {
      LOG(ERROR) << "Filepath is empty";
      return false;
    }
    // Return fake fd for unit tests.
    fd = base::ScopedFD(dup(STDERR_FILENO));
    return true;
  }
};

class XuCameraServiceTest : public ::testing::Test {
 public:
  XuCameraServiceTest() = default;
  XuCameraServiceTest(const XuCameraServiceTest&) = delete;
  XuCameraServiceTest& operator=(const XuCameraServiceTest&) = delete;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    XuCameraService::InitializeForTesting(new TestDelegate());
  }

  void TearDown() override {
    XuCameraService::Shutdown();
    CfmHotlineClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  // Returns a mojo::Remote for the mojom::XuCamera by faking the
  // way the cfm mojom binder daemon would request it through chrome.
  const mojo::Remote<mojom::XuCamera>& GetXuCameraRemote() {
    if (!XuCameraService::IsInitialized()) {
      XuCameraService::InitializeForTesting(&delegate_);
    }
    if (xu_camera_remote_.is_bound()) {
      return xu_camera_remote_;
    }

    // if there is no valid remote create one
    auto* interface_name = mojom::XuCamera::Name_;

    base::RunLoop run_loop;

    // Fake out CfmServiceContext
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
                pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver_set_.Add(&context_, std::move(pending_receiver));
        }));

    context_.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<chromeos::cfm::mojom::CfmServiceAdaptor>
                pending_adaptor_remote,
            chromeos::cfm::mojom::CfmServiceContext::ProvideAdaptorCallback
                callback) {
          ASSERT_EQ(interface_name, service_id);
          adaptor_remote_.Bind(std::move(pending_adaptor_remote));
          std::move(callback).Run(true);
        }));

    EXPECT_TRUE(GetClient()->FakeEmitSignal(interface_name));
    run_loop.RunUntilIdle();

    EXPECT_TRUE(adaptor_remote_.is_connected());

    adaptor_remote_->OnBindService(
        xu_camera_remote_.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(xu_camera_remote_.is_connected());

    return xu_camera_remote_;
  }

 protected:
  FakeCfmServiceContext context_;
  mojo::Remote<mojom::XuCamera> xu_camera_remote_;
  mojo::ReceiverSet<chromeos::cfm::mojom::CfmServiceContext>
      context_receiver_set_;
  mojo::Remote<chromeos::cfm::mojom::CfmServiceAdaptor> adaptor_remote_;
  FakeServiceConnectionImpl fake_service_connection_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestDelegate delegate_;
};

// This test ensures that the XuCameraService is discoverable by its
// mojom name by sending a signal received by CfmHotlineClient.
TEST_F(XuCameraServiceTest, XuCameraServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::XuCamera::Name_));
}

// This test ensures that the XuCameraService correctly registers itself
// for discovery by the cfm mojom binder daemon and correctly returns a
// working mojom remote.
TEST_F(XuCameraServiceTest, GetXuCameraRemote) {
  ASSERT_TRUE(GetXuCameraRemote().is_connected());
}

// This test ensure that the XU camera can get unit id
TEST_F(XuCameraServiceTest, GetXuCameraUnitId) {
  base::RunLoop run_loop;
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  GetXuCameraRemote()->GetUnitId(
      /* id= */ std::move(devPath), /* guid= */ kGuid,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const uint8_t unit_id) {
            EXPECT_EQ(error_code, ENOSYS);
            EXPECT_EQ(unit_id, '0');
            run_loop.Quit();
          }));
  run_loop.Run();
}

// This test ensure that the XU camera can map control
TEST_F(XuCameraServiceTest, GetXuCameraMapCtrl) {
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  auto mapping = mojom::ControlMapping::New(
      /* id= */ 1, /* name= */ kName, /* guid= */ kGuid, /* selector= */ 1,
      /* size= */ 1, /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED, /* menu_entries= */
      kMenuEntries->Clone());
  base::RunLoop run_loop;
  GetXuCameraRemote()->MapCtrl(
      /* id= */ std::move(devPath), /* mapping_ctrl= */ std::move(mapping),
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraGetCtrlWithDeviceIdCtrlMapping) {
  base::RunLoop run_loop;
  auto devId = mojom::WebcamId::NewDeviceId("123");
  auto mapping = mojom::CtrlType::NewMappingCtrl(mojom::ControlMapping::New(
      /* id= */ 1, /* name= */ kName, /* guid= */ kGuid, /* selector= */ 1,
      /* size= */ 1, /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED, /* menu_entries= */
      kMenuEntries->Clone()));
  GetXuCameraRemote()->GetCtrl(
      /* id= */ std::move(devId), /* ctrl= */ std::move(mapping),
      /* fn= */ mojom::GetFn::kCur,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            const std::vector<uint8_t> vec;
            EXPECT_EQ(error_code, ENOENT);
            EXPECT_EQ(data, vec);
            run_loop.Quit();
          }));
  run_loop.Run();
}

// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraGetCtrlWithDevPathCtrlMapping) {
  base::RunLoop run_loop;
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  auto mapping = mojom::CtrlType::NewMappingCtrl(mojom::ControlMapping::New(
      /* id= */ 1, /* name= */ kName, /* guid= */ kGuid, /* selector= */ 1,
      /* size= */ 1, /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED, /* menu_entries= */
      kMenuEntries->Clone()));
  GetXuCameraRemote()->GetCtrl(
      /* id= */ std::move(devPath), /* ctrl= */ std::move(mapping),
      /* fn= */ mojom::GetFn::kCur,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            EXPECT_EQ(error_code, 0);
            EXPECT_EQ(data, kValueAsUint8);
            run_loop.Quit();
          }));
  run_loop.Run();
}

// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraGetCtrlWithDeviceIdCtrlQuery) {
  base::RunLoop run_loop;
  auto devId = mojom::WebcamId::NewDeviceId("123");
  auto ctrlTypeQuery =
      mojom::CtrlType::NewQueryCtrl(mojom::ControlQuery::New(1, 1));
  std::vector<uint8_t> data{'a', 'b', 'c'};
  GetXuCameraRemote()->GetCtrl(
      /* id= */ std::move(devId), /* ctrl= */ std::move(ctrlTypeQuery),
      /* fn= */ mojom::GetFn::kCur,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            const std::vector<uint8_t> vec;
            EXPECT_EQ(error_code, ENOENT);
            EXPECT_EQ(data, vec);
            run_loop.Quit();
          }));
  run_loop.Run();
}

// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraGetCtrlWithDevPathCtrlQuery) {
  base::RunLoop run_loop;
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  auto ctrlTypeQuery =
      mojom::CtrlType::NewQueryCtrl(mojom::ControlQuery::New(1, 1));
  GetXuCameraRemote()->GetCtrl(
      /* id= */ std::move(devPath), /* ctrl= */ std::move(ctrlTypeQuery),
      /* fn= */ mojom::GetFn::kCur,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            EXPECT_EQ(error_code, 0);
            EXPECT_EQ(data, kData);
            run_loop.Quit();
          }));
  run_loop.Run();
}

// This test ensure that the XU camera can get control length given a ctrl
// query
TEST_F(XuCameraServiceTest, XuCameraGetCtrlLenWithDevPathCtrlQuery) {
  base::RunLoop run_loop;
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  auto ctrlTypeQuery =
      mojom::CtrlType::NewQueryCtrl(mojom::ControlQuery::New(1, 1));
  GetXuCameraRemote()->GetCtrl(
      /* id= */ std::move(devPath), /* ctrl= */ std::move(ctrlTypeQuery),
      /* fn= */ mojom::GetFn::kLen,
      base::BindLambdaForTesting(
          [&](const uint8_t error_code, const std::vector<uint8_t>& data) {
            EXPECT_EQ(error_code, 0);
            EXPECT_EQ(data, kLen);
            run_loop.Quit();
          }));
  run_loop.Run();
}

/// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraSetCtrlWithDeviceIdCtrlMapping) {
  base::RunLoop run_loop;
  auto devId = mojom::WebcamId::NewDeviceId("123");
  auto mapping = mojom::CtrlType::NewMappingCtrl(mojom::ControlMapping::New(
      /* id= */ 1, /* name= */ kName, /* guid= */ kGuid, /* selector= */ 1,
      /* size= */ 1, /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED, /* menu_entries= */
      kMenuEntries->Clone()));
  std::vector<uint8_t> data{'a', 'b', 'c'};
  GetXuCameraRemote()->SetCtrl(
      /* id= */ std::move(devId), /* ctrl= */ std::move(mapping),
      /* data= */ data,
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, ENOENT);
        run_loop.Quit();
      }));
  run_loop.Run();
}

/// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraSetCtrlWithDevPathCtrlMapping) {
  base::RunLoop run_loop;
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  auto mapping = mojom::CtrlType::NewMappingCtrl(mojom::ControlMapping::New(
      /* id= */ 1, /* name= */ kName, /* guid= */ kGuid, /* selector= */ 1,
      /* size= */ 1, /* offset= */ 1,
      /* v4l2_type= */ V4L2_CTRL_TYPE_INTEGER,
      /* data_type= */ UVC_CTRL_DATA_TYPE_SIGNED, /* menu_entries= */
      kMenuEntries->Clone()));
  std::vector<uint8_t> data{'a', 'b', 'c'};
  GetXuCameraRemote()->SetCtrl(
      /* id= */ std::move(devPath), /* ctrl= */ std::move(mapping),
      /* data= */ data,
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

/// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraSetCtrlWithDeviceIdCtrlQuery) {
  base::RunLoop run_loop;
  auto devId = mojom::WebcamId::NewDeviceId("123");
  auto ctrlTypeQuery =
      mojom::CtrlType::NewQueryCtrl(mojom::ControlQuery::New(1, 1));
  std::vector<uint8_t> data{'a', 'b', 'c'};
  GetXuCameraRemote()->SetCtrl(
      /* id= */ std::move(devId), /* ctrl= */ std::move(ctrlTypeQuery),
      /* data= */ data,
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, ENOENT);
        run_loop.Quit();
      }));
  run_loop.Run();
}

/// This test ensure that the XU camera can get control given a ctrl query
TEST_F(XuCameraServiceTest, XuCameraSetCtrlWithDevPathCtrlQuery) {
  base::RunLoop run_loop;
  auto devPath = mojom::WebcamId::NewDevPath(kFakePath);
  auto ctrlTypeQuery =
      mojom::CtrlType::NewQueryCtrl(mojom::ControlQuery::New(1, 1));
  std::vector<uint8_t> data{'a', 'b', 'c'};
  GetXuCameraRemote()->SetCtrl(
      /* id= */ std::move(devPath), /* ctrl= */ std::move(ctrlTypeQuery),
      /* data= */ data,
      base::BindLambdaForTesting([&](const uint8_t error_code) {
        EXPECT_EQ(error_code, 0);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace ash::cfm
