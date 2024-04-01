// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <initguid.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

namespace media {

class MockD3D11VideoDecoderImpl : public D3D11VideoDecoderImpl {
 public:
  MockD3D11VideoDecoderImpl(MockD3D11VideoDecoderImpl** thiz)
      : D3D11VideoDecoderImpl(
            nullptr,
            base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>()) {
    *thiz = this;
  }

  void Initialize(InitCB init_cb) override { MockInitialize(); }

  MOCK_METHOD0(MockInitialize, void());
};

class D3D11VideoDecoderTest : public ::testing::Test {
 public:
  const std::pair<uint16_t, uint16_t> LegacyIntelGPU = {0x8086, 0x102};
  const std::pair<uint16_t, uint16_t> RecentIntelGPU = {0x8086, 0x100};
  const std::pair<uint16_t, uint16_t> LegacyAMDGPU = {0x1022, 0x130f};
  const std::pair<uint16_t, uint16_t> RecentAMDGPU = {0x1022, 0x130e};

  void SetUp() override {
    // Set up some sane defaults.
    gpu_preferences_.enable_zero_copy_dxgi_video = true;
    gpu_preferences_.use_passthrough_cmd_decoder = false;
    gpu_workarounds_.disable_dxgi_zero_copy_video = false;
    gpu_task_runner_ = task_environment_.GetMainThreadTaskRunner();

    // Create a mock D3D11 device that supports 11.0.  Note that if you change
    // this, then you probably also want VideoDevice1 and friends, below.
    mock_d3d11_device_ = MakeComPtr<NiceMock<D3D11DeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), GetFeatureLevel)
        .WillByDefault(Return(D3D_FEATURE_LEVEL_11_0));

    mock_d3d11_device_context_ = MakeComPtr<D3D11DeviceContextMock>();
    ON_CALL(*mock_d3d11_device_.Get(), GetImmediateContext(_))
        .WillByDefault(SetComPointee<0>(mock_d3d11_device_context_.Get()));

    // Set up an D3D11VideoDevice rather than ...Device1, since Initialize uses
    // Device for checking decoder GUIDs.
    // TODO(liberato): Try to use Device1 more often.
    mock_d3d11_video_device_ = MakeComPtr<NiceMock<D3D11VideoDeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_ID3D11VideoDevice, _))
        .WillByDefault(
            SetComPointeeAndReturnOk<1>(mock_d3d11_video_device_.Get()));

    mock_multithreaded_ = MakeComPtr<NiceMock<D3D11MultithreadMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_ID3D11Multithread, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(mock_multithreaded_.Get()));

    EnableDecoder(D3D11_DECODER_PROFILE_H264_VLD_NOFGT);

    mock_d3d11_video_decoder_ = MakeComPtr<D3D11VideoDecoderMock>();
    ON_CALL(*mock_d3d11_video_device_.Get(), CreateVideoDecoder(_, _, _))
        .WillByDefault(
            SetComPointeeAndReturnOk<2>(mock_d3d11_video_decoder_.Get()));

    mock_d3d11_video_context_ = MakeComPtr<D3D11VideoContextMock>();
    ON_CALL(*mock_d3d11_device_context_.Get(),
            QueryInterface(IID_ID3D11VideoContext, _))
        .WillByDefault(
            SetComPointeeAndReturnOk<1>(mock_d3d11_video_context_.Get()));

    SetUpAdapters();
  }

  void SetGPUProfile(std::pair<uint16_t, uint16_t> vendor_and_gpu) {
    mock_adapter_desc_.DeviceId = static_cast<UINT>(vendor_and_gpu.second);
    mock_adapter_desc_.VendorId = static_cast<UINT>(vendor_and_gpu.first);

    ON_CALL(*mock_dxgi_adapter_.Get(), GetDesc(_))
        .WillByDefault(
            DoAll(SetArgPointee<0>(mock_adapter_desc_), Return(S_OK)));
  }

  void SetUpAdapters() {
    mock_dxgi_device_ = MakeComPtr<NiceMock<DXGIDeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_IDXGIDevice, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(mock_dxgi_device_.Get()));

    mock_dxgi_adapter_ = MakeComPtr<NiceMock<DXGIAdapterMock>>();
    ON_CALL(*mock_dxgi_device_.Get(), GetAdapter(_))
        .WillByDefault(SetComPointeeAndReturnOk<0>(mock_dxgi_adapter_.Get()));

    SetGPUProfile(RecentIntelGPU);
  }

  // Enable a decoder for the given GUID.  Only one decoder may be enabled at a
  // time.  GUIDs are things like D3D11_DECODER_PROFILE_H264_VLD_NOFGT or
  // D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0, etc.
  void EnableDecoder(GUID decoder_profile) {
    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderProfileCount())
        .WillByDefault(Return(1));

    // Note that we don't check if the guid in the config actually matches
    // |decoder_profile|.  Perhaps we should.
    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderProfile(0, _))
        .WillByDefault(DoAll(SetArgPointee<1>(decoder_profile), Return(S_OK)));

    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderConfigCount(_, _))
        .WillByDefault(DoAll(
            Invoke(this, &D3D11VideoDecoderTest::GetVideoDecoderConfigCount),
            Return(S_OK)));

    video_decoder_config_.ConfigBitstreamRaw =
        decoder_profile == D3D11_DECODER_PROFILE_H264_VLD_NOFGT ? 2 : 1;

    ON_CALL(*mock_d3d11_video_device_.Get(), GetVideoDecoderConfig(_, 0, _))
        .WillByDefault(
            DoAll(SetArgPointee<2>(video_decoder_config_), Return(S_OK)));
  }

  void GetVideoDecoderConfigCount(const D3D11_VIDEO_DECODER_DESC* desc,
                                  UINT* config_count_out) {
    last_video_decoder_desc_ = *desc;
    *config_count_out = 1;
  }

  // Most recently provided video decoder desc.
  absl::optional<D3D11_VIDEO_DECODER_DESC> last_video_decoder_desc_;
  D3D11_VIDEO_DECODER_CONFIG video_decoder_config_;

  void TearDown() override {
    decoder_.reset();
    // Run the gpu thread runner to tear down |impl_|.
    base::RunLoop().RunUntilIdle();
  }

  void EnableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndDisableFeature(feature);
  }

  // If provided, |supported_configs| is the list of configs that will be
  // checked before init can succeed.  If one is provided, then we'll
  // use it.  Otherwise, we'll use the list that's autodetected by the
  // decoder based on the current device mock.
  void CreateDecoder(
      absl::optional<D3D11VideoDecoder::SupportedConfigs> supported_configs =
          absl::optional<D3D11VideoDecoder::SupportedConfigs>()) {
    auto get_device_cb = base::BindRepeating(
        [](Microsoft::WRL::ComPtr<ID3D11Device> device) { return device; },
        mock_d3d11_device_);

    // Autodetect the supported configs, unless it's being overridden.
    if (!supported_configs) {
      supported_configs = D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
          gpu_preferences_, gpu_workarounds_, get_device_cb);
    }
    base::SequenceBound<MockD3D11VideoDecoderImpl> impl(gpu_task_runner_,
                                                        &impl_);
    task_environment_.RunUntilIdle();

    // We store it in a std::unique_ptr<VideoDecoder> so that the default
    // deleter works.  The dtor is protected.
    decoder_ = base::WrapUnique<VideoDecoder>(
        d3d11_decoder_raw_ = new D3D11VideoDecoder(
            gpu_task_runner_, std::make_unique<NullMediaLog>(),
            gpu_preferences_, gpu_workarounds_, std::move(impl),
            base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>(),
            get_device_cb, *supported_configs, is_hdr_supported_));
  }

  void InitializeDecoder(const VideoDecoderConfig& config, bool expectSuccess) {
    const bool low_delay = false;
    CdmContext* cdm_context = nullptr;

    if (expectSuccess) {
      EXPECT_CALL(*this, MockInitCB(_)).Times(0);
      EXPECT_CALL(*impl_, MockInitialize());
    } else {
      EXPECT_CALL(*this, MockInitCB(_)).Times(1);
    }
    decoder_->Initialize(config, low_delay, cdm_context,
                         base::BindOnce(&D3D11VideoDecoderTest::CheckStatus,
                                        base::Unretained(this), expectSuccess),
                         base::DoNothing(), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void CheckStatus(bool expectSuccess, Status actual) {
    ASSERT_EQ(expectSuccess, actual.is_ok());
    MockInitCB(actual);
  }

  MOCK_METHOD1(MockInitCB, void(Status));

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  std::unique_ptr<VideoDecoder> decoder_;
  D3D11VideoDecoder* d3d11_decoder_raw_ = nullptr;
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  MockD3D11VideoDecoderImpl* impl_ = nullptr;

  Microsoft::WRL::ComPtr<D3D11DeviceMock> mock_d3d11_device_;
  Microsoft::WRL::ComPtr<D3D11DeviceContextMock> mock_d3d11_device_context_;
  Microsoft::WRL::ComPtr<D3D11MultithreadMock> mock_multithreaded_;
  Microsoft::WRL::ComPtr<D3D11VideoDeviceMock> mock_d3d11_video_device_;
  Microsoft::WRL::ComPtr<D3D11VideoDecoderMock> mock_d3d11_video_decoder_;
  Microsoft::WRL::ComPtr<D3D11VideoContextMock> mock_d3d11_video_context_;
  Microsoft::WRL::ComPtr<DXGIDeviceMock> mock_dxgi_device_;
  Microsoft::WRL::ComPtr<DXGIAdapterMock> mock_dxgi_adapter_;

  // Used by CreateDecoder() to tell D3D11VideoDecoder about HDR support.
  bool is_hdr_supported_ = true;

  DXGI_ADAPTER_DESC mock_adapter_desc_;

  absl::optional<base::test::ScopedFeatureList> scoped_feature_list_;
  base::win::ScopedCOMInitializer com_initializer_;
};

TEST_F(D3D11VideoDecoderTest, SupportsVP9Profile0WithDecoderEnabled) {
  VideoDecoderConfig configuration = TestVideoConfig::NormalCodecProfile(
      VideoCodec::kVP9, VP9PROFILE_PROFILE0);

  EnableDecoder(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0);
  CreateDecoder();
  // We don't support vp9 on windows 7 and below.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    InitializeDecoder(configuration, false);
  } else {
    InitializeDecoder(configuration, true);
  }
}

TEST_F(D3D11VideoDecoderTest, DoesNotSupportVP9WithLegacyGPU) {
  SetGPUProfile(LegacyIntelGPU);
  VideoDecoderConfig configuration = TestVideoConfig::NormalCodecProfile(
      VideoCodec::kVP9, VP9PROFILE_PROFILE0);

  EnableDecoder(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0);
  CreateDecoder();
  InitializeDecoder(configuration, false);
}

TEST_F(D3D11VideoDecoderTest, DoesNotSupportVP9WithGPUWorkaroundDisableVPX) {
  gpu_workarounds_.disable_accelerated_vp9_decode = true;
  VideoDecoderConfig configuration = TestVideoConfig::NormalCodecProfile(
      VideoCodec::kVP9, VP9PROFILE_PROFILE0);

  EnableDecoder(D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0);
  CreateDecoder();
  InitializeDecoder(configuration, false);
}

TEST_F(D3D11VideoDecoderTest, DoesNotSupportVP9WithoutDecoderEnabled) {
  VideoDecoderConfig configuration = TestVideoConfig::NormalCodecProfile(
      VideoCodec::kVP9, VP9PROFILE_PROFILE0);

  // Enable a non-VP9 decoder.
  EnableDecoder(D3D11_DECODER_PROFILE_H264_VLD_NOFGT);  // Paranoia, not VP9.
  CreateDecoder();
  InitializeDecoder(configuration, false);
}

TEST_F(D3D11VideoDecoderTest, DoesNotSupportsH264HIGH10Profile) {
  CreateDecoder();

  VideoDecoderConfig high10 = TestVideoConfig::NormalCodecProfile(
      VideoCodec::kH264, H264PROFILE_HIGH10PROFILE);

  InitializeDecoder(high10, false);
}

TEST_F(D3D11VideoDecoderTest, SupportsH264WithAutodetectedConfig) {
  CreateDecoder();

  VideoDecoderConfig normal =
      TestVideoConfig::NormalCodecProfile(VideoCodec::kH264, H264PROFILE_MAIN);

  InitializeDecoder(normal, true);
  // TODO(liberato): Check |last_video_decoder_desc_| for sanity.
}

TEST_F(D3D11VideoDecoderTest, DoesNotSupportH264IfNoSupportedConfig) {
  // This is identical to SupportsH264, except that we initialize with an empty
  // list of supported configs.  This should match nothing.  Assuming that
  // SupportsH264WithSupportedConfig passes, then this checks that the supported
  // config check kinda works.
  // For whatever reason, Optional<SupportedConfigs>({}) results in one that
  // doesn't have a value, rather than one that has an empty vector.
  absl::optional<D3D11VideoDecoder::SupportedConfigs> empty_configs;
  empty_configs.emplace(std::vector<SupportedVideoDecoderConfig>());
  CreateDecoder(empty_configs);

  VideoDecoderConfig normal =
      TestVideoConfig::NormalCodecProfile(VideoCodec::kH264, H264PROFILE_MAIN);

  InitializeDecoder(normal, false);
}

TEST_F(D3D11VideoDecoderTest, DoesNotSupportEncryptedConfig) {
  CreateDecoder();
  VideoDecoderConfig encrypted_config =
      TestVideoConfig::NormalCodecProfile(VideoCodec::kH264, H264PROFILE_MAIN);
  encrypted_config.SetIsEncrypted(true);
  InitializeDecoder(encrypted_config, false);
}

TEST_F(D3D11VideoDecoderTest, IgnoreWorkaroundsIgnoresWorkaround) {
  // k...IgnoreWorkarounds should enable the decoder even if it's turned off
  // for gpu workarounds.
  EnableFeature(kD3D11VideoDecoderIgnoreWorkarounds);
  gpu_workarounds_.disable_d3d11_video_decoder = true;
  CreateDecoder();
  InitializeDecoder(
      TestVideoConfig::NormalCodecProfile(VideoCodec::kH264, H264PROFILE_MAIN),
      true);
}

TEST_F(D3D11VideoDecoderTest, WorkaroundTurnsOffDecoder) {
  //  We shouldn't be able to decode if the decoder is off via gpu workaround.
  gpu_workarounds_.disable_d3d11_video_decoder = true;
  CreateDecoder();
  InitializeDecoder(
      TestVideoConfig::NormalCodecProfile(VideoCodec::kH264, H264PROFILE_MAIN),
      false);
}

}  // namespace media
