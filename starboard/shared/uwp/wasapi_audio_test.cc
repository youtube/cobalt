// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/uwp/wasapi_audio.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace uwp {

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

class IAudioClientMock : public IAudioClient2 {
 public:
  MOCK_METHOD1(SetClientProperties,
               HRESULT(const AudioClientProperties* props));
  MOCK_METHOD1(GetMixFormat, HRESULT(WAVEFORMATEX** format));
  MOCK_METHOD2(QueryInterface, HRESULT(const IID& riid, void** obj));
  MOCK_METHOD0(AddRef, ULONG());
  MOCK_METHOD0(Release, ULONG());
  MOCK_METHOD6(Initialize,
               HRESULT(AUDCLNT_SHAREMODE s,
                       DWORD d,
                       REFERENCE_TIME t1,
                       REFERENCE_TIME t2,
                       const WAVEFORMATEX* w,
                       LPCGUID id));
  MOCK_METHOD1(GetBufferSize, HRESULT(UINT32* n));
  MOCK_METHOD1(GetStreamLatency, HRESULT(REFERENCE_TIME* t));
  MOCK_METHOD1(GetCurrentPadding, HRESULT(UINT32* n));
  MOCK_METHOD3(IsFormatSupported,
               HRESULT(AUDCLNT_SHAREMODE, const WAVEFORMATEX*, WAVEFORMATEX**));
  MOCK_METHOD2(GetDevicePeriod, HRESULT(REFERENCE_TIME*, REFERENCE_TIME*));
  MOCK_METHOD0(Start, HRESULT());
  MOCK_METHOD0(Stop, HRESULT());
  MOCK_METHOD0(Reset, HRESULT());
  MOCK_METHOD1(SetEventHandle, HRESULT(HANDLE h));
  MOCK_METHOD2(GetService, HRESULT(const IID& riid, void** obj));
  MOCK_METHOD2(IsOffloadCapable, HRESULT(AUDIO_STREAM_CATEGORY asc, BOOL* b));
  MOCK_METHOD4(GetBufferSizeLimits,
               HRESULT(const WAVEFORMATEX* w,
                       BOOL b,
                       REFERENCE_TIME* t1,
                       REFERENCE_TIME* t2));
};

class IUnknownMock : public IUnknown {
 public:
  MOCK_METHOD2(QueryInterface, HRESULT(const IID& riid, void** obj));
  MOCK_METHOD0(AddRef, ULONG());
  MOCK_METHOD0(Release, ULONG());
};

class AsyncOperationMock : public IActivateAudioInterfaceAsyncOperation {
 public:
  MOCK_METHOD2(GetActivateResult,
               HRESULT(HRESULT* activateResult, IUnknown** activatedInterface));
  MOCK_METHOD2(QueryInterface, HRESULT(const IID& riid, void** obj));
  MOCK_METHOD0(AddRef, ULONG());
  MOCK_METHOD0(Release, ULONG());
};

class WASAPIAudioDeviceTest : public testing::Test {
 protected:
  void InitAudioDevice() {
    audio_device_->bitrate_ = -1;
    audio_device_->channels_ = -1;
    audio_device_->audio_client_ = nullptr;
    audio_device_->activate_completed_ =
        CreateEvent(nullptr, TRUE, FALSE, nullptr);
  }

  virtual void SetUp() {
    audio_device_ = Microsoft::WRL::Make<WASAPIAudioDevice>();
    InitAudioDevice();
  }

  void CallActivateCompleted() {
    audio_device_->ActivateCompleted(&async_operation_);
  }

  bool IsInitializedAndNotChanged() {
    CallActivateCompleted();

    bool res = (audio_device_->GetBitrate() == -1);
    res &= (audio_device_->GetNumChannels() == -1);

    return res;
  }

  WAVEFORMATEX* CreateWAVEFORMATEX(int test_channels,
                                   int test_samples_per_sec,
                                   int test_bits_per_sample) {
    WAVEFORMATEX* format =
        reinterpret_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
    EXPECT_NE(nullptr, format);

    format->nChannels = test_channels;
    format->nSamplesPerSec = test_samples_per_sec;
    format->wBitsPerSample = test_bits_per_sample;
    return format;
  }

  AsyncOperationMock async_operation_;
  IAudioClientMock client_;
  IUnknownMock audio_interface_;
  Microsoft::WRL::ComPtr<WASAPIAudioDevice> audio_device_;
};

// GetActivateResult returns not S_OK, activateResult (first argument) is set to
// S_OK, activation fails, both GetNumChannels and GetBitrate should return -1
TEST_F(WASAPIAudioDeviceTest, GetActivateResultFailsReturnValue) {
  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(S_OK), Return(E_FAIL)));

  EXPECT_TRUE(IsInitializedAndNotChanged());
}

// GetActivateResult returns S_OK, activateResult (first argument) is not S_OK,
// activation fails, both GetNumChannels and GetBitrate should return -1
TEST_F(WASAPIAudioDeviceTest, GetActivateResultFailsFirstArg) {
  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(E_ACCESSDENIED), Return(S_OK)));

  EXPECT_TRUE(IsInitializedAndNotChanged());
}

// QueryInterface of audio_interface_ returns not S_OK as returned value,
// activation fails, both GetNumChannels and GetBitrate should return -1
TEST_F(WASAPIAudioDeviceTest, QueryInterfaceIsNotSupported) {
  EXPECT_CALL(audio_interface_, QueryInterface(_, _))
      .WillRepeatedly(Return(E_NOINTERFACE));

  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(S_OK),
                            SetArgPointee<1>(&audio_interface_), Return(S_OK)));

  EXPECT_TRUE(IsInitializedAndNotChanged());
}

// QueryInterface of audio_interface_ returns null pointer to interface object,
// activation fails, both GetNumChannels and GetBitrate should return -1
TEST_F(WASAPIAudioDeviceTest, QueryInterfaceReturnsNullObj) {
  IAudioClientMock* p_client = nullptr;

  // If ppvObject of audio_interface_ is NULL, QueryInterface returns E_POINTER
  EXPECT_CALL(audio_interface_, QueryInterface(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(p_client), Return(E_POINTER)));

  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(S_OK),
                            SetArgPointee<1>(&audio_interface_), Return(S_OK)));

  EXPECT_TRUE(IsInitializedAndNotChanged());
}

// SetClientProperties of audio_client_ returns not S_OK,
// activation fails, both GetNumChannels and GetBitrate should return -1
TEST_F(WASAPIAudioDeviceTest, SetClientPropertiesFails) {
  EXPECT_CALL(client_, SetClientProperties(_)).WillRepeatedly(Return(E_FAIL));

  EXPECT_CALL(audio_interface_, QueryInterface(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&client_), Return(S_OK)));

  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(S_OK),
                            SetArgPointee<1>(&audio_interface_), Return(S_OK)));

  EXPECT_TRUE(IsInitializedAndNotChanged());
}

// GetMixFormat of audio_client_ returns not S_OK,
// activation fails, both GetNumChannels and GetBitrate should return -1
TEST_F(WASAPIAudioDeviceTest, GetMixFormatFails) {
  EXPECT_CALL(client_, SetClientProperties(_)).WillRepeatedly(Return(S_OK));
  EXPECT_CALL(client_, GetMixFormat(_))
      .WillOnce(Return(E_POINTER))
      .RetiresOnSaturation();
  EXPECT_CALL(client_, GetMixFormat(_))
      .WillOnce(Return(E_OUTOFMEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(client_, GetMixFormat(_))
      .WillOnce(Return(AUDCLNT_E_SERVICE_NOT_RUNNING))
      .RetiresOnSaturation();
  EXPECT_CALL(client_, GetMixFormat(_))
      .WillOnce(Return(AUDCLNT_E_DEVICE_INVALIDATED))
      .RetiresOnSaturation();

  EXPECT_CALL(audio_interface_, QueryInterface(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&client_), Return(S_OK)));

  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(S_OK),
                            SetArgPointee<1>(&audio_interface_), Return(S_OK)));

  for (int i = 0; i < 4; i++) {
    InitAudioDevice();
    EXPECT_TRUE(IsInitializedAndNotChanged());
  }
}

// GetMixFormat of audio_client_ returns S_OK and sets some values,
// activation is OK, to check GetNumChannels and GetBitrate returns
// some numbers
TEST_F(WASAPIAudioDeviceTest, CheckValuesReturned) {
  EXPECT_CALL(client_, SetClientProperties(_)).WillRepeatedly(Return(S_OK));

  EXPECT_CALL(audio_interface_, QueryInterface(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(&client_), Return(S_OK)));

  EXPECT_CALL(async_operation_, GetActivateResult(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(S_OK),
                            SetArgPointee<1>(&audio_interface_), Return(S_OK)));

  int test_channels_arr[] = {2, 6};
  int test_samples_per_sec_arr[] = {44100, 48000, 96000};
  int test_bits_per_sample_arr[] = {16, 24};

  for (int test_channels : test_channels_arr) {
    for (int test_samples_per_sec : test_samples_per_sec_arr) {
      for (int test_bits_per_sample : test_bits_per_sample_arr) {
        EXPECT_CALL(client_, GetMixFormat(_))
            .WillRepeatedly(DoAll(
                SetArgPointee<0>(CreateWAVEFORMATEX(
                    test_channels, test_samples_per_sec, test_bits_per_sample)),
                Return(S_OK)));

        InitAudioDevice();
        CallActivateCompleted();
        EXPECT_EQ(audio_device_->GetNumChannels(), test_channels);
        EXPECT_EQ(audio_device_->GetBitrate(),
                  (test_samples_per_sec * test_bits_per_sample));
      }  // for k
    }    // for j
  }      // for i
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
