// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_UWP_WASAPI_AUDIO_SINK_H_
#define STARBOARD_SHARED_UWP_WASAPI_AUDIO_SINK_H_

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl\client.h>

#include <functional>
#include <memory>
#include <queue>

#include "starboard/common/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/wasapi_include.h"

namespace starboard {
namespace shared {
namespace uwp {

typedef enum __MIDL___MIDL_itf_mmdeviceapi_0000_0000_0001 {
  eRender = 0,
  eCapture,
  eAll,
  EDataFlow_enum_count
} EDataFlow;

typedef enum __MIDL___MIDL_itf_mmdeviceapi_0000_0000_0002 {
  eConsole = 0,
  eMultimedia,
  eCommunications,
  ERole_enum_count
} ERole;

MIDL_INTERFACE("D666063F-1587-4E43-81F1-B948E807363F")
IMMDevice : public IUnknown {
 public:
  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Activate(
      /* [annotation][in] */
      _In_ REFIID iid,
      /* [annotation][in] */
      _In_ DWORD dwClsCtx,
      /* [annotation][unique][in] */
      _In_opt_ PROPVARIANT * pActivationParams,
      /* [annotation][iid_is][out] */
      _Out_ void** ppInterface) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE OpenPropertyStore(
      /* [annotation][in] */
      _In_ DWORD stgmAccess,
      /* [annotation][out] */
      _Out_ IPropertyStore * *ppProperties) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetId(
      /* [annotation][out] */
      _Outptr_ LPWSTR * ppstrId) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetState(
      /* [annotation][out] */
      _Out_ DWORD * pdwState) = 0;
};

MIDL_INTERFACE("A95664D2-9614-4F35-A746-DE8DB63617E6")
IMMDeviceEnumerator : public IUnknown {
 public:
  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(
      /* [annotation][in] */
      _In_ EDataFlow dataFlow,
      /* [annotation][in] */
      _In_ DWORD dwStateMask,
      /* [annotation][out] */
      _Out_ IMMDeviceCollection * *ppDevices) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE
  GetDefaultAudioEndpoint(
      /* [annotation][in] */
      _In_ EDataFlow dataFlow,
      /* [annotation][in] */
      _In_ ERole role,
      /* [annotation][out] */
      _Out_ IMMDevice * *ppEndpoint) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetDevice(
      /* [annotation][in] */
      _In_ LPCWSTR pwstrId,
      /* [annotation][out] */
      _Out_ IMMDevice * *ppDevice) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE
      RegisterEndpointNotificationCallback(
          /* [annotation][in] */
          _In_ IMMNotificationClient * pClient) = 0;

  virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE
      UnregisterEndpointNotificationCallback(
          /* [annotation][in] */
          _In_ IMMNotificationClient * pClient) = 0;
};

const IID IID_IAudioClock = __uuidof(IAudioClock);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_ISimpleAudioVolume = __uuidof(ISimpleAudioVolume);
class DECLSPEC_UUID("BCDE0395-E52F-467C-8E3D-C4579291692E") MMDeviceEnumerator;
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

using ::starboard::shared::starboard::player::DecodedAudio;
using ::starboard::shared::starboard::player::JobQueue;
using ::starboard::shared::starboard::player::JobThread;

class WASAPIAudioSink {
 public:
  WASAPIAudioSink();
  ~WASAPIAudioSink() {}

  bool Initialize(int channels, int sample_rate, SbMediaAudioCodec audio_codec);

  bool WriteBuffer(scoped_refptr<DecodedAudio> decoded_audio);

  void Reset();
  void Pause();
  void Play();
  void SetVolume(double volume);
  void SetPlaybackRate(double playback_rate);
  // GetCurrentPlaybackTime() can be called from any thread.
  double GetCurrentPlaybackTime(uint64_t* updated_at);

  bool playing() { return !paused_.load() && playback_rate_.load() > 0.0; }

 private:
  void OutputFrames();
  void UpdatePlaybackState();

  const int kMaxDecodedAudios = 16;

  // The size of one (E)AC3 sync frame in IEC 61937 frames.
  const int kAc3BufferSizeInFrames = 1536;
  const int kEac3BufferSizeInFrames = 6144;

  atomic_bool paused_{false};
  atomic_double playback_rate_{0.0};
  atomic_double volume_{0.0};
  double current_volume_ = 0.0;
  bool was_playing_ = false;

  Microsoft::WRL::ComPtr<IMMDevice> device_;
  Microsoft::WRL::ComPtr<IAudioClient3> audio_client_;
  Microsoft::WRL::ComPtr<IAudioRenderClient> render_client_;
  Microsoft::WRL::ComPtr<ISimpleAudioVolume> audio_volume_;

  Mutex audio_clock_mutex_;
  Microsoft::WRL::ComPtr<IAudioClock> audio_clock_;
  uint64_t audio_clock_frequency_;

  uint32_t client_buffer_size_in_frames_ = 0;
  int frames_per_audio_buffer_;

  Mutex output_frames_mutex_;
  std::queue<scoped_refptr<DecodedAudio>> pending_decoded_audios_;

  std::unique_ptr<JobThread> job_thread_;

  starboard::ThreadChecker thread_checker_;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_WASAPI_AUDIO_SINK_H_
