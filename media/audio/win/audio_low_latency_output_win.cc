// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_output_win.h"

#include <Functiondiscoverykeys_devpkey.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;

namespace media {

WASAPIAudioOutputStream::WASAPIAudioOutputStream(AudioManagerWin* manager,
                                                 const AudioParameters& params,
                                                 ERole device_role)
    : com_init_(ScopedCOMInitializer::kMTA),
      creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      render_thread_(NULL),
      opened_(false),
      started_(false),
      restart_rendering_mode_(false),
      volume_(1.0),
      endpoint_buffer_size_frames_(0),
      device_role_(device_role),
      num_written_frames_(0),
      source_(NULL) {
  CHECK(com_init_.succeeded());
  DCHECK(manager_);

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the avrt.dll";

  // Set up the desired render format specified by the client.
  format_.nSamplesPerSec = params.sample_rate();
  format_.wFormatTag = WAVE_FORMAT_PCM;
  format_.wBitsPerSample = params.bits_per_sample();
  format_.nChannels = params.channels();
  format_.nBlockAlign = (format_.wBitsPerSample / 8) * format_.nChannels;
  format_.nAvgBytesPerSec = format_.nSamplesPerSec * format_.nBlockAlign;
  format_.cbSize = 0;

  // Size in bytes of each audio frame.
  frame_size_ = format_.nBlockAlign;

  // Store size (in different units) of audio packets which we expect to
  // get from the audio endpoint device in each render event.
  packet_size_frames_ = params.GetBytesPerBuffer() / format_.nBlockAlign;
  packet_size_bytes_ = params.GetBytesPerBuffer();
  packet_size_ms_ = (1000.0 * packet_size_frames_) / params.sample_rate();
  DVLOG(1) << "Number of bytes per audio frame  : " << frame_size_;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;
  DVLOG(1) << "Number of milliseconds per packet: " << packet_size_ms_;

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_render_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_render_event_.IsValid());

  // Create the event which will be set when a stream switch shall take place.
  stream_switch_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stream_switch_event_.IsValid());
}

WASAPIAudioOutputStream::~WASAPIAudioOutputStream() {}

bool WASAPIAudioOutputStream::Open() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (opened_)
    return true;

  // Create an IMMDeviceEnumerator interface and obtain a reference to
  // the IMMDevice interface of the default rendering device with the
  // specified role.
  HRESULT hr = SetRenderDevice(device_role_);
  if (FAILED(hr)) {
    return false;
  }

  // Obtain an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  hr = ActivateRenderDevice();
  if (FAILED(hr)) {
    return false;
  }

  // Retrieve the stream format which the audio engine uses for its internal
  // processing/mixing of shared-mode streams.
  hr = GetAudioEngineStreamFormat();
  if (FAILED(hr)) {
    return false;
  }

  // Verify that the selected audio endpoint supports the specified format
  // set during construction.
  if (!DesiredFormatIsSupported()) {
    return false;
  }

  // Initialize the audio stream between the client and the device using
  // shared mode and a lowest possible glitch-free latency.
  hr = InitializeAudioEngine();
  if (FAILED(hr)) {
    return false;
  }

  // Register this client as an IMMNotificationClient implementation.
  // Only OnDefaultDeviceChanged() and OnDeviceStateChanged() and are
  // non-trivial.
  hr = device_enumerator_->RegisterEndpointNotificationCallback(this);

  opened_ = true;
  return SUCCEEDED(hr);
}

void WASAPIAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  DCHECK(callback);
  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return;

  if (started_)
    return;

  if (restart_rendering_mode_) {
    // The selected audio device has been removed or disabled and a new
    // default device has been enabled instead. The current implementation
    // does not to support this sequence of events. Given that Open()
    // and Start() are usually called in one sequence; it should be a very
    // rare event.
    // TODO(henrika): it is possible to extend the functionality here.
    LOG(ERROR) << "Unable to start since the selected default device has "
                  "changed since Open() was called.";
    return;
  }

  source_ = callback;

  // Avoid start-up glitches by filling up the endpoint buffer with "silence"
  // before starting the stream.
  BYTE* data_ptr = NULL;
  HRESULT hr = audio_render_client_->GetBuffer(endpoint_buffer_size_frames_,
                                               &data_ptr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to use rendering audio buffer: " << std::hex << hr;
    return;
  }

  // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
  // explicitly write silence data to the rendering buffer.
  audio_render_client_->ReleaseBuffer(endpoint_buffer_size_frames_,
                                      AUDCLNT_BUFFERFLAGS_SILENT);
  num_written_frames_ = endpoint_buffer_size_frames_;

  // Sanity check: verify that the endpoint buffer is filled with silence.
  UINT32 num_queued_frames = 0;
  audio_client_->GetCurrentPadding(&num_queued_frames);
  DCHECK(num_queued_frames == num_written_frames_);

  // Create and start the thread that will drive the rendering by waiting for
  // render events.
  render_thread_ = new base::DelegateSimpleThread(this, "wasapi_render_thread");
  render_thread_->Start();
  if (!render_thread_->HasBeenStarted()) {
    DLOG(ERROR) << "Failed to start WASAPI render thread.";
    return;
  }

  // Start streaming data between the endpoint buffer and the audio engine.
  hr = audio_client_->Start();
  if (FAILED(hr)) {
    SetEvent(stop_render_event_.Get());
    render_thread_->Join();
    render_thread_ = NULL;
    HandleError(hr);
    return;
  }

  started_ = true;
}

void WASAPIAudioOutputStream::Stop() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (!started_)
    return;

  // Shut down the render thread.
  if (stop_render_event_.IsValid()) {
    SetEvent(stop_render_event_.Get());
  }

  // Stop output audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to stop output streaming: " << std::hex << hr;
  }

  // Wait until the thread completes and perform cleanup.
  if (render_thread_) {
    SetEvent(stop_render_event_.Get());
    render_thread_->Join();
    render_thread_ = NULL;
  }

  // Flush all pending data and reset the audio clock stream position to 0.
  hr = audio_client_->Reset();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to reset streaming: " << std::hex << hr;
  }

  // Extra safety check to ensure that the buffers are cleared.
  // If the buffers are not cleared correctly, the next call to Start()
  // would fail with AUDCLNT_E_BUFFER_ERROR at IAudioRenderClient::GetBuffer().
  UINT32 num_queued_frames = 0;
  audio_client_->GetCurrentPadding(&num_queued_frames);
  DCHECK_EQ(0u, num_queued_frames);

  // Ensure that we don't quit the main thread loop immediately next
  // time Start() is called.
  ResetEvent(stop_render_event_.Get());

  started_ = false;
}

void WASAPIAudioOutputStream::Close() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  if (opened_ && device_enumerator_) {
    // De-register the IMMNotificationClient callback interface.
    HRESULT hr = device_enumerator_->UnregisterEndpointNotificationCallback(
        this);
    DLOG_IF(ERROR, FAILED(hr)) << "Failed to disable device notifications: "
                               << std::hex << hr;
  }

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void WASAPIAudioOutputStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  float volume_float = static_cast<float>(volume);
  if (volume_float < 0.0f || volume_float > 1.0f) {
    return;
  }
  volume_ = volume_float;
}

void WASAPIAudioOutputStream::GetVolume(double* volume) {
  DVLOG(1) << "GetVolume()";
  *volume = static_cast<double>(volume_);
}

// static
int WASAPIAudioOutputStream::HardwareSampleRate(ERole device_role) {
  // It is assumed that this static method is called from a COM thread, i.e.,
  // CoInitializeEx() is not called here again to avoid STA/MTA conflicts.
  ScopedComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                 NULL,
                                 CLSCTX_INPROC_SERVER,
                                 __uuidof(IMMDeviceEnumerator),
                                 enumerator.ReceiveVoid());
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << std::hex << hr;
    return 0.0;
  }

  ScopedComPtr<IMMDevice> endpoint_device;
  hr = enumerator->GetDefaultAudioEndpoint(eRender,
                                           device_role,
                                           endpoint_device.Receive());
  if (FAILED(hr)) {
    // This will happen if there's no audio output device found or available
    // (e.g. some audio cards that have outputs will still report them as
    // "not found" when no speaker is plugged into the output jack).
    LOG(WARNING) << "No audio end point: " << std::hex << hr;
    return 0.0;
  }

  ScopedComPtr<IAudioClient> audio_client;
  hr = endpoint_device->Activate(__uuidof(IAudioClient),
                                 CLSCTX_INPROC_SERVER,
                                 NULL,
                                 audio_client.ReceiveVoid());
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << std::hex << hr;
    return 0.0;
  }

  base::win::ScopedCoMem<WAVEFORMATEX> audio_engine_mix_format;
  hr = audio_client->GetMixFormat(&audio_engine_mix_format);
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << std::hex << hr;
    return 0.0;
  }

  return static_cast<int>(audio_engine_mix_format->nSamplesPerSec);
}

void WASAPIAudioOutputStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Increase the thread priority.
  render_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  DWORD task_index = 0;
  HANDLE mm_task = avrt::AvSetMmThreadCharacteristics(L"Pro Audio",
                                                      &task_index);
  bool mmcss_is_ok =
      (mm_task && avrt::AvSetMmThreadPriority(mm_task, AVRT_PRIORITY_CRITICAL));
  if (!mmcss_is_ok) {
    // Failed to enable MMCSS on this thread. It is not fatal but can lead
    // to reduced QoS at high load.
    DWORD err = GetLastError();
    LOG(WARNING) << "Failed to enable MMCSS (error code=" << err << ").";
  }

  HRESULT hr = S_FALSE;

  bool playing = true;
  bool error = false;
  HANDLE wait_array[] = { stop_render_event_,
                          stream_switch_event_,
                          audio_samples_render_event_ };
  UINT64 device_frequency = 0;

  // The IAudioClock interface enables us to monitor a stream's data
  // rate and the current position in the stream. Allocate it before we
  // start spinning.
  ScopedComPtr<IAudioClock> audio_clock;
  hr = audio_client_->GetService(__uuidof(IAudioClock),
                                 audio_clock.ReceiveVoid());
  if (SUCCEEDED(hr)) {
    // The device frequency is the frequency generated by the hardware clock in
    // the audio device. The GetFrequency() method reports a constant frequency.
    hr = audio_clock->GetFrequency(&device_frequency);
  }
  error = FAILED(hr);
  PLOG_IF(ERROR, error) << "Failed to acquire IAudioClock interface: "
                        << std::hex << hr;

  // Keep rendering audio until the stop event or the stream-switch event
  // is signaled. An error event can also break the main thread loop.
  while (playing && !error) {
    // Wait for a close-down event, stream-switch event or a new render event.
    DWORD wait_result = WaitForMultipleObjects(arraysize(wait_array),
                                               wait_array,
                                               FALSE,
                                               INFINITE);

    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_render_event_| has been set.
        playing = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |stream_switch_event_| has been set. Stop rendering and try to
        // re-start the session using a new endpoint device.
        if (!RestartRenderingUsingNewDefaultDevice()) {
          // Abort the thread since stream switching failed.
          playing = false;
          error = true;
        }
        break;
      case WAIT_OBJECT_0 + 2:
        {
          // |audio_samples_render_event_| has been set.
          UINT32 num_queued_frames = 0;
          uint8* audio_data = NULL;

          // Get the padding value which represents the amount of rendering
          // data that is queued up to play in the endpoint buffer.
          hr = audio_client_->GetCurrentPadding(&num_queued_frames);

          // Determine how much new data we can write to the buffer without
          // the risk of overwriting previously written data that the audio
          // engine has not yet read from the buffer.
          size_t num_available_frames =
              endpoint_buffer_size_frames_ - num_queued_frames;

          // Check if there is enough available space to fit the packet size
          // specified by the client.
          if (FAILED(hr) || (num_available_frames < packet_size_frames_))
            continue;

          // Derive the number of packets we need get from the client to
          // fill up the available area in the endpoint buffer.
          size_t num_packets = (num_available_frames / packet_size_frames_);

          // Get data from the client/source.
          for (size_t n = 0; n < num_packets; ++n) {
            // Grab all available space in the rendering endpoint buffer
            // into which the client can write a data packet.
            hr = audio_render_client_->GetBuffer(packet_size_frames_,
                                                 &audio_data);
            if (FAILED(hr)) {
              DLOG(ERROR) << "Failed to use rendering audio buffer: "
                          << std::hex << hr;
              continue;
            }

            // Derive the audio delay which corresponds to the delay between
            // a render event and the time when the first audio sample in a
            // packet is played out through the speaker. This delay value
            // can typically be utilized by an acoustic echo-control (AEC)
            // unit at the render side.
            UINT64 position = 0;
            int audio_delay_bytes = 0;
            hr = audio_clock->GetPosition(&position, NULL);
            if (SUCCEEDED(hr)) {
              // Stream position of the sample that is currently playing
              // through the speaker.
              double pos_sample_playing_frames = format_.nSamplesPerSec *
                  (static_cast<double>(position) / device_frequency);

              // Stream position of the last sample written to the endpoint
              // buffer. Note that, the packet we are about to receive in
              // the upcoming callback is also included.
              size_t pos_last_sample_written_frames =
                  num_written_frames_ + packet_size_frames_;

              // Derive the actual delay value which will be fed to the
              // render client using the OnMoreData() callback.
              audio_delay_bytes = (pos_last_sample_written_frames -
                  pos_sample_playing_frames) *  frame_size_;
            }

            // Read a data packet from the registered client source and
            // deliver a delay estimate in the same callback to the client.
            // A time stamp is also stored in the AudioBuffersState. This
            // time stamp can be used at the client side to compensate for
            // the delay between the usage of the delay value and the time
            // of generation.
            uint32 num_filled_bytes = source_->OnMoreData(
                audio_data, packet_size_bytes_,
                AudioBuffersState(0, audio_delay_bytes));

            // Perform in-place, software-volume adjustments.
            media::AdjustVolume(audio_data,
                                num_filled_bytes,
                                format_.nChannels,
                                format_.wBitsPerSample >> 3,
                                volume_);

            // Zero out the part of the packet which has not been filled by
            // the client. Using silence is the least bad option in this
            // situation.
            if (num_filled_bytes < packet_size_bytes_) {
              memset(&audio_data[num_filled_bytes], 0,
                    (packet_size_bytes_ - num_filled_bytes));
            }

            // Release the buffer space acquired in the GetBuffer() call.
            DWORD flags = 0;
            audio_render_client_->ReleaseBuffer(packet_size_frames_,
                                                flags);

            num_written_frames_ += packet_size_frames_;
          }
        }
        break;
      default:
        error = true;
        break;
    }
  }

  if (playing && error) {
    // Stop audio rendering since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    audio_client_->Stop();
    PLOG(ERROR) << "WASAPI rendering failed.";
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }
}

void WASAPIAudioOutputStream::HandleError(HRESULT err) {
  NOTREACHED() << "Error code: " << std::hex << err;
  if (source_)
    source_->OnError(this, static_cast<int>(err));
}

HRESULT WASAPIAudioOutputStream::SetRenderDevice(ERole device_role) {
  // Create the IMMDeviceEnumerator interface.
  HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                 NULL,
                                 CLSCTX_INPROC_SERVER,
                                 __uuidof(IMMDeviceEnumerator),
                                 device_enumerator_.ReceiveVoid());
  if (SUCCEEDED(hr)) {
    // Retrieve the default render audio endpoint for the specified role.
    // Note that, in Windows Vista, the MMDevice API supports device roles
    // but the system-supplied user interface programs do not.
    hr = device_enumerator_->GetDefaultAudioEndpoint(
        eRender, device_role, endpoint_device_.Receive());
    if (FAILED(hr))
      return hr;

    // Verify that the audio endpoint device is active. That is, the audio
    // adapter that connects to the endpoint device is present and enabled.
    DWORD state = DEVICE_STATE_DISABLED;
    hr = endpoint_device_->GetState(&state);
    if (SUCCEEDED(hr)) {
      if (!(state & DEVICE_STATE_ACTIVE)) {
        DLOG(ERROR) << "Selected render device is not active.";
        hr = E_ACCESSDENIED;
      }
    }
  }

  return hr;
}

HRESULT WASAPIAudioOutputStream::ActivateRenderDevice() {
  // Creates and activates an IAudioClient COM object given the selected
  // render endpoint device.
  HRESULT hr = endpoint_device_->Activate(__uuidof(IAudioClient),
                                          CLSCTX_INPROC_SERVER,
                                          NULL,
                                          audio_client_.ReceiveVoid());
  return hr;
}

HRESULT WASAPIAudioOutputStream::GetAudioEngineStreamFormat() {
  // Retrieve the stream format that the audio engine uses for its internal
  // processing/mixing of shared-mode streams.
  return audio_client_->GetMixFormat(&audio_engine_mix_format_);
}

bool WASAPIAudioOutputStream::DesiredFormatIsSupported() {
  // In shared mode, the audio engine always supports the mix format,
  // which is stored in the |audio_engine_mix_format_| member. In addition,
  // the audio engine *might* support similar formats that have the same
  // sample rate and number of channels as the mix format but differ in
  // the representation of audio sample values.
  base::win::ScopedCoMem<WAVEFORMATEX> closest_match;
  HRESULT hr = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                                &format_,
                                                &closest_match);
  DLOG_IF(ERROR, hr == S_FALSE) << "Format is not supported "
                                << "but a closest match exists.";
  return (hr == S_OK);
}

HRESULT WASAPIAudioOutputStream::InitializeAudioEngine() {
  // TODO(henrika): this buffer scheme is still under development.
  // The exact details are yet to be determined based on tests with different
  // audio clients.
  int glitch_free_buffer_size_ms = static_cast<int>(packet_size_ms_ + 0.5);
  if (audio_engine_mix_format_->nSamplesPerSec == 48000) {
    // Initial tests have shown that we have to add 10 ms extra to
    // ensure that we don't run empty for any packet size.
    glitch_free_buffer_size_ms += 10;
  } else if (audio_engine_mix_format_->nSamplesPerSec == 44100) {
    // Initial tests have shown that we have to add 20 ms extra to
    // ensure that we don't run empty for any packet size.
    glitch_free_buffer_size_ms += 20;
  } else {
    glitch_free_buffer_size_ms += 20;
  }
  DVLOG(1) << "glitch_free_buffer_size_ms: " << glitch_free_buffer_size_ms;
  REFERENCE_TIME requested_buffer_duration_hns =
      static_cast<REFERENCE_TIME>(glitch_free_buffer_size_ms * 10000);

  // Initialize the audio stream between the client and the device.
  // We connect indirectly through the audio engine by using shared mode
  // and WASAPI is initialized in an event driven mode.
  // Note that this API ensures that the buffer is never smaller than the
  // minimum buffer size needed to ensure glitch-free rendering.
  // If we requests a buffer size that is smaller than the audio engine's
  // minimum required buffer size, the method sets the buffer size to this
  // minimum buffer size rather than to the buffer size requested.
  HRESULT hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                         AUDCLNT_STREAMFLAGS_NOPERSIST,
                                         requested_buffer_duration_hns,
                                         0,
                                         &format_,
                                         NULL);
  if (FAILED(hr))
    return hr;

  // Retrieve the length of the endpoint buffer shared between the client
  // and the audio engine. The buffer length the buffer length determines
  // the maximum amount of rendering data that the client can write to
  // the endpoint buffer during a single processing pass.
  // A typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
  hr = audio_client_->GetBufferSize(&endpoint_buffer_size_frames_);
  if (FAILED(hr))
    return hr;
  DVLOG(1) << "endpoint buffer size: " << endpoint_buffer_size_frames_
           << " [frames]";
#ifndef NDEBUG
  // The period between processing passes by the audio engine is fixed for a
  // particular audio endpoint device and represents the smallest processing
  // quantum for the audio engine. This period plus the stream latency between
  // the buffer and endpoint device represents the minimum possible latency
  // that an audio application can achieve in shared mode.
  REFERENCE_TIME default_device_period = 0;
  REFERENCE_TIME minimum_device_period = 0;
  HRESULT hr_dbg = audio_client_->GetDevicePeriod(&default_device_period,
                                                  &minimum_device_period);
  if (SUCCEEDED(hr_dbg)) {
    // Shared mode device period.
    DVLOG(1) << "default device period: "
             << static_cast<double>(default_device_period / 10000.0)
             << " [ms]";
    // Exclusive mode device period.
    DVLOG(1) << "minimum device period: "
             << static_cast<double>(minimum_device_period / 10000.0)
             << " [ms]";
  }

  REFERENCE_TIME latency = 0;
  hr_dbg = audio_client_->GetStreamLatency(&latency);
  if (SUCCEEDED(hr_dbg)) {
    DVLOG(1) << "stream latency: " << static_cast<double>(latency / 10000.0)
             << " [ms]";
  }
#endif

  // Set the event handle that the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  hr = audio_client_->SetEventHandle(audio_samples_render_event_.Get());
  if (FAILED(hr))
    return hr;

  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  // The methods in this interface manage the movement of data packets
  // that contain audio-rendering data.
  hr = audio_client_->GetService(__uuidof(IAudioRenderClient),
                                 audio_render_client_.ReceiveVoid());
  return hr;
}

ULONG WASAPIAudioOutputStream::AddRef() {
  NOTREACHED() << "IMMNotificationClient should not use this method.";
  return 1;
}

ULONG WASAPIAudioOutputStream::Release() {
  NOTREACHED() << "IMMNotificationClient should not use this method.";
  return 1;
}

HRESULT WASAPIAudioOutputStream::QueryInterface(REFIID iid, void** object) {
  NOTREACHED() << "IMMNotificationClient should not use this method.";
  if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
    *object = static_cast < IMMNotificationClient*>(this);
  } else {
    return E_NOINTERFACE;
  }
  return S_OK;
}

STDMETHODIMP WASAPIAudioOutputStream::OnDeviceStateChanged(LPCWSTR device_id,
                                                      DWORD new_state) {
#ifndef NDEBUG
  std::string device_name = GetDeviceName(device_id);
  std::string device_state;

  switch (new_state) {
    case DEVICE_STATE_ACTIVE:
      device_state = "ACTIVE";
      break;
    case DEVICE_STATE_DISABLED:
      device_state = "DISABLED";
      break;
    case DEVICE_STATE_NOTPRESENT:
      device_state = "NOTPRESENT";
      break;
    case DEVICE_STATE_UNPLUGGED:
      device_state = "UNPLUGGED";
      break;
    default:
      break;
  }

  DVLOG(1) << "-> State changed to " << device_state
           << " for device: " << device_name;
#endif
  return S_OK;
}

HRESULT WASAPIAudioOutputStream::OnDefaultDeviceChanged(EDataFlow flow,
    ERole role, LPCWSTR new_default_device_id) {
  if (new_default_device_id == NULL) {
    // The user has removed or disabled the default device for our
    // particular role, and no other device is available to take that role.
    DLOG(ERROR) << "All devices are disabled.";
    return E_FAIL;
  }

  if (flow ==  eRender && role == device_role_) {
    // Log the name of the new default device for our configured role.
    std::string new_default_device = GetDeviceName(new_default_device_id);
    DVLOG(1) << "-> New default device: "  << new_default_device;

    // Initiate a stream switch if not already initiated by signaling the
    // stream-switch event to inform the render thread that it is OK to
    // re-initialize the active audio renderer. All the action takes place
    // on the WASAPI render thread.
    if (!restart_rendering_mode_) {
      restart_rendering_mode_ = true;
      SetEvent(stream_switch_event_.Get());
    }
  }

  return S_OK;
}

std::string WASAPIAudioOutputStream::GetDeviceName(LPCWSTR device_id) const {
  std::string name;
  ScopedComPtr<IMMDevice> audio_device;

  // Get the IMMDevice interface corresponding to the given endpoint ID string.
  HRESULT hr = device_enumerator_->GetDevice(device_id, audio_device.Receive());
  if (SUCCEEDED(hr)) {
    // Retrieve user-friendly name of endpoint device.
    // Example: "Speakers (Realtek High Definition Audio)".
    ScopedComPtr<IPropertyStore> properties;
    hr = audio_device->OpenPropertyStore(STGM_READ, properties.Receive());
    if (SUCCEEDED(hr)) {
      PROPVARIANT friendly_name;
      PropVariantInit(&friendly_name);
      hr = properties->GetValue(PKEY_Device_FriendlyName, &friendly_name);
      if (SUCCEEDED(hr) && friendly_name.vt == VT_LPWSTR) {
        if (friendly_name.pwszVal)
          name = WideToUTF8(friendly_name.pwszVal);
      }
      PropVariantClear(&friendly_name);
    }
  }
  return name;
}

bool WASAPIAudioOutputStream::RestartRenderingUsingNewDefaultDevice() {
  DCHECK(base::PlatformThread::CurrentId() == render_thread_->tid());
  DCHECK(restart_rendering_mode_);

  // The |restart_rendering_mode_| event has been signaled which means that
  // we must stop the current renderer and start a new render session using
  // the new default device with the configured role.

  // Stop the current rendering.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    restart_rendering_mode_ = false;
    return false;
  }

  // Release acquired interfaces (IAudioRenderClient, IAudioClient, IMMDevice).
  audio_render_client_.Release();
  audio_client_.Release();
  endpoint_device_.Release();

  // Retrieve the new default render audio endpoint (IMMDevice) for the
  // specified role.
  hr = device_enumerator_->GetDefaultAudioEndpoint(
      eRender, device_role_, endpoint_device_.Receive());
  if (FAILED(hr)) {
    restart_rendering_mode_ = false;
    return false;
  }

  // Re-create an IAudioClient interface.
  hr = ActivateRenderDevice();
  if (FAILED(hr)) {
    restart_rendering_mode_ = false;
    return false;
  }

  // Retrieve the new mix format and ensure that it is supported given
  // the predefined format set at construction.
  base::win::ScopedCoMem<WAVEFORMATEX> new_audio_engine_mix_format;
  hr = audio_client_->GetMixFormat(&new_audio_engine_mix_format);
  if (FAILED(hr) || !DesiredFormatIsSupported()) {
    restart_rendering_mode_ = false;
    return false;
  }

  // Re-initialize the audio engine using the new audio endpoint.
  // This method will create a new IAudioRenderClient interface.
  hr = InitializeAudioEngine();
  if (FAILED(hr)) {
    restart_rendering_mode_ = false;
    return false;
  }

  // All released interfaces (IAudioRenderClient, IAudioClient, IMMDevice)
  // are now re-initiated and it is now possible to re-start audio rendering.

  // Start rendering again using the new default audio endpoint.
  hr = audio_client_->Start();

  restart_rendering_mode_ = false;
  return SUCCEEDED(hr);
}

}  // namespace media
