// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_unified_win.h"

#include <Functiondiscoverykeys_devpkey.h>

#include "base/debug/trace_event.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;
using base::win::ScopedCoMem;

namespace media {

static HRESULT GetMixFormat(EDataFlow data_flow, WAVEFORMATEX** device_format) {
  // It is assumed that this static method is called from a COM thread, i.e.,
  // CoInitializeEx() is not called here again to avoid STA/MTA conflicts.
  ScopedComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                enumerator.ReceiveVoid());
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << std::hex << hr;
    return hr;
  }

  ScopedComPtr<IMMDevice> endpoint_device;
  hr = enumerator->GetDefaultAudioEndpoint(data_flow,
                                           eConsole,
                                           endpoint_device.Receive());
  if (FAILED(hr)) {
    LOG(WARNING) << "No audio end point: " << std::hex << hr;
    return hr;
  }

  ScopedComPtr<IAudioClient> audio_client;
  hr = endpoint_device->Activate(__uuidof(IAudioClient),
                                 CLSCTX_INPROC_SERVER,
                                 NULL,
                                 audio_client.ReceiveVoid());
  DCHECK(SUCCEEDED(hr)) << "Failed to activate device: " << std::hex << hr;
  if (SUCCEEDED(hr)) {
    // Retrieve the stream format that the audio engine uses for its internal
    // processing/mixing of shared-mode streams.
    hr = audio_client->GetMixFormat(device_format);
    DCHECK(SUCCEEDED(hr)) << "GetMixFormat: " << std::hex << hr;
  }

  return hr;
}

static ScopedComPtr<IMMDevice> CreateDefaultAudioDevice(EDataFlow data_flow) {
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator;
  ScopedComPtr<IMMDevice> endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                device_enumerator.ReceiveVoid());
  if (SUCCEEDED(hr)) {
    // Retrieve the default render audio endpoint for the specified role.
    hr = device_enumerator->GetDefaultAudioEndpoint(
        data_flow, eConsole, endpoint_device.Receive());

    if (FAILED(hr)) {
      PLOG(ERROR) << "GetDefaultAudioEndpoint: " << std::hex << hr;
      return endpoint_device;
    }

    // Verify that the audio endpoint device is active. That is, the audio
    // adapter that connects to the endpoint device is present and enabled.
    DWORD state = DEVICE_STATE_DISABLED;
    hr = endpoint_device->GetState(&state);
    if (SUCCEEDED(hr)) {
      if (!(state & DEVICE_STATE_ACTIVE)) {
        PLOG(ERROR) << "Selected render device is not active.";
        endpoint_device.Release();
      }
    }
  }

  return endpoint_device;
}

static ScopedComPtr<IAudioClient> CreateAudioClient(IMMDevice* audio_device) {
  ScopedComPtr<IAudioClient> audio_client;

  // Creates and activates an IAudioClient COM object given the selected
  // endpoint device.
  HRESULT hr = audio_device->Activate(__uuidof(IAudioClient),
                                      CLSCTX_INPROC_SERVER,
                                      NULL,
                                      audio_client.ReceiveVoid());
  PLOG_IF(ERROR, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  return audio_client;
}

static bool IsFormatSupported(IAudioClient* audio_client,
                              WAVEFORMATPCMEX* format) {
  ScopedCoMem<WAVEFORMATEXTENSIBLE> closest_match;
  HRESULT hr = audio_client->IsFormatSupported(
      AUDCLNT_SHAREMODE_SHARED, reinterpret_cast<WAVEFORMATEX*>(format),
      reinterpret_cast<WAVEFORMATEX**>(&closest_match));

  // This log can only be triggered for shared mode.
  DLOG_IF(ERROR, hr == S_FALSE) << "Format is not supported "
      << "but a closest match exists.";
  // This log can be triggered both for shared and exclusive modes.
  DLOG_IF(ERROR, hr == AUDCLNT_E_UNSUPPORTED_FORMAT) << "Unsupported format.";
  if (hr == S_FALSE) {
    DVLOG(1) << "wFormatTag    : " << closest_match->Format.wFormatTag;
    DVLOG(1) << "nChannels     : " << closest_match->Format.nChannels;
    DVLOG(1) << "nSamplesPerSec: " << closest_match->Format.nSamplesPerSec;
    DVLOG(1) << "wBitsPerSample: " << closest_match->Format.wBitsPerSample;
  }

  return (hr == S_OK);
}

// Get the default scheduling period for a shared-mode stream in a specified
// direction. Note that the period between processing passes by the audio
// engine is fixed for a particular audio endpoint device and represents the
// smallest processing quantum for the audio engine.
static REFERENCE_TIME GetAudioEngineDevicePeriod(EDataFlow data_flow) {
  ScopedComPtr<IMMDevice> endpoint_device = CreateDefaultAudioDevice(data_flow);
  if (!endpoint_device)
     return 0;

  ScopedComPtr<IAudioClient> audio_client;
  audio_client = CreateAudioClient(endpoint_device);
  if (!audio_client)
    return 0;

  REFERENCE_TIME default_device_period = 0;
  REFERENCE_TIME minimum_device_period = 0;

  // Times are expressed in 100-nanosecond units.
  HRESULT hr = audio_client->GetDevicePeriod(&default_device_period,
                                             &minimum_device_period);
  if (SUCCEEDED(hr)) {
    std::string flow = (data_flow == eCapture) ? "[in] " : "[out] ";
    DVLOG(1) << flow << "default_device_period: " << default_device_period;
    DVLOG(1) << flow << "minimum_device_period: " << minimum_device_period;

    return default_device_period;
  }

  return 0;
}

WASAPIUnifiedStream::WASAPIUnifiedStream(AudioManagerWin* manager,
                                         const AudioParameters& params)
    : creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      audio_io_thread_(NULL),
      opened_(false),
      endpoint_render_buffer_size_frames_(0),
      endpoint_capture_buffer_size_frames_(0),
      source_(NULL),
      capture_bus_(AudioBus::Create(params)),
      render_bus_(AudioBus::Create(params)) {
  DCHECK(manager_);

  LOG_IF(ERROR, !HasUnifiedDefaultIO())
      << "Unified audio I/O is not supported.";

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the avrt.dll";

  // Begin with the WAVEFORMATEX structure that specifies the basic format.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels =  params.channels();
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = params.bits_per_sample();
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE.
  format_.Samples.wValidBitsPerSample = params.bits_per_sample();
  format_.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  // Store size (in different units) of audio packets which we expect to
  // get from the audio endpoint device in each render event.
  packet_size_frames_ = params.GetBytesPerBuffer() / format->nBlockAlign;
  float packet_size_ms = (1000.0 * packet_size_frames_) / params.sample_rate();
  DVLOG(1) << "Number of bytes per audio frame  : " << format->nBlockAlign;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;
  DVLOG(1) << "Number of milliseconds per packet: " << packet_size_ms;

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time a buffer
  // has been recorded.
  capture_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));

  // Create the event which will be set in Stop() when straeming shall stop.
  stop_streaming_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
}

WASAPIUnifiedStream::~WASAPIUnifiedStream() {
}

bool WASAPIUnifiedStream::Open() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (opened_)
    return true;

  if (!HasUnifiedDefaultIO()) {
    LOG(ERROR) << "Unified audio I/O is not supported.";
    return false;
  }

  // Render side:
  //   IMMDeviceEnumerator -> IMMDevice
  //   IMMDevice -> IAudioClient
  //   IAudioClient -> IAudioRenderClient

  ScopedComPtr<IMMDevice> render_device = CreateDefaultAudioDevice(eRender);
  if (!render_device)
     return false;

  ScopedComPtr<IAudioClient> audio_output_client =
      CreateAudioClient(render_device);
  if (!audio_output_client)
    return false;

  if (!IsFormatSupported(audio_output_client, &format_))
    return false;

  ScopedComPtr<IAudioRenderClient> audio_render_client =
      CreateAudioRenderClient(audio_output_client);
  if (!audio_render_client)
    return false;

  // Capture side:
  //   IMMDeviceEnumerator -> IMMDevice
  //   IMMDevice -> IAudioClient
  //   IAudioClient -> IAudioCaptureClient

  ScopedComPtr<IMMDevice> capture_device = CreateDefaultAudioDevice(eCapture);
  if (!capture_device)
    return false;

  ScopedComPtr<IAudioClient> audio_input_client =
      CreateAudioClient(capture_device);
  if (!audio_input_client)
    return false;

  if (!IsFormatSupported(audio_input_client, &format_))
    return false;

  ScopedComPtr<IAudioCaptureClient> audio_capture_client =
      CreateAudioCaptureClient(audio_input_client);
  if (!audio_capture_client)
    return false;

  // Set the event handle that the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  HRESULT hr = audio_input_client->SetEventHandle(capture_event_.Get());
  if (FAILED(hr))
    return false;

  // Store all valid COM interfaces.
  audio_output_client_ = audio_output_client;
  audio_render_client_ = audio_render_client;
  audio_input_client_ = audio_input_client;
  audio_capture_client_ = audio_capture_client;

  opened_ = true;
  return SUCCEEDED(hr);
}

void WASAPIUnifiedStream::Start(AudioSourceCallback* callback) {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  CHECK(callback);
  CHECK(opened_);

  if (audio_io_thread_.get()) {
    CHECK_EQ(callback, source_);
    return;
  }

  source_ = callback;

  // Avoid start-up glitches by filling up the endpoint buffer with "silence"
  // before starting the stream.
  BYTE* data_ptr = NULL;
  HRESULT hr = audio_render_client_->GetBuffer(
      endpoint_render_buffer_size_frames_, &data_ptr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to use rendering audio buffer: " << std::hex << hr;
    return;
  }

  // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
  // explicitly write silence data to the rendering buffer.
  audio_render_client_->ReleaseBuffer(endpoint_render_buffer_size_frames_,
                                      AUDCLNT_BUFFERFLAGS_SILENT);

  // Sanity check: verify that the endpoint buffer is filled with silence.
  UINT32 num_queued_frames = 0;
  audio_output_client_->GetCurrentPadding(&num_queued_frames);
  DCHECK(num_queued_frames == endpoint_render_buffer_size_frames_);

  // Create and start the thread that will capturing and rendering.
  audio_io_thread_.reset(
      new base::DelegateSimpleThread(this, "wasapi_io_thread"));
  audio_io_thread_->Start();
  if (!audio_io_thread_->HasBeenStarted()) {
    DLOG(ERROR) << "Failed to start WASAPI IO thread.";
    return;
  }

  // Start input streaming data between the endpoint buffer and the audio
  // engine.
  hr = audio_input_client_->Start();
  if (FAILED(hr)) {
    StopAndJoinThread(hr);
    return;
  }

  // Start output streaming data between the endpoint buffer and the audio
  // engine.
  hr = audio_output_client_->Start();
  if (FAILED(hr)) {
    StopAndJoinThread(hr);
    return;
  }
}

void WASAPIUnifiedStream::Stop() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (!audio_io_thread_.get())
    return;

  // Stop input audio streaming.
  HRESULT hr = audio_input_client_->Stop();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
      << "Failed to stop input streaming: " << std::hex << hr;
  }

  // Stop output audio streaming.
  hr = audio_output_client_->Stop();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to stop output streaming: " << std::hex << hr;
  }

  // Wait until the thread completes and perform cleanup.
  SetEvent(stop_streaming_event_.Get());
  audio_io_thread_->Join();
  audio_io_thread_.reset();

  // Ensure that we don't quit the main thread loop immediately next
  // time Start() is called.
  ResetEvent(stop_streaming_event_.Get());

  // Clear source callback, it'll be set again on the next Start() call.
  source_ = NULL;

  // Flush all pending data and reset the audio clock stream position to 0.
  hr = audio_output_client_->Reset();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to reset output streaming: " << std::hex << hr;
  }

  audio_input_client_->Reset();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to reset input streaming: " << std::hex << hr;
  }

  // Extra safety check to ensure that the buffers are cleared.
  // If the buffers are not cleared correctly, the next call to Start()
  // would fail with AUDCLNT_E_BUFFER_ERROR at IAudioRenderClient::GetBuffer().
  // This check is is only needed for shared-mode streams.
  UINT32 num_queued_frames = 0;
  audio_output_client_->GetCurrentPadding(&num_queued_frames);
  DCHECK_EQ(0u, num_queued_frames);
}

void WASAPIUnifiedStream::Close() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void WASAPIUnifiedStream::SetVolume(double volume) {
  NOTIMPLEMENTED();
}

void WASAPIUnifiedStream::GetVolume(double* volume) {
  NOTIMPLEMENTED();
}

// static
bool WASAPIUnifiedStream::HasUnifiedDefaultIO() {
  int output_size = HardwareBufferSize(eRender);
  int input_size = HardwareBufferSize(eCapture);
  int output_channels = HardwareChannelCount(eRender);
  int input_channels = HardwareChannelCount(eCapture);
  return ((output_size == input_size) && (output_channels == input_channels));
}

// static
int WASAPIUnifiedStream::HardwareChannelCount(EDataFlow data_flow) {
  base::win::ScopedCoMem<WAVEFORMATPCMEX> format_ex;
  HRESULT hr = GetMixFormat(
      data_flow, reinterpret_cast<WAVEFORMATEX**>(&format_ex));
  if (FAILED(hr))
    return 0;

  // Number of channels in the stream. Corresponds to the number of bits
  // set in the dwChannelMask.
  std::string flow = (data_flow == eCapture) ? "[in] " : "[out] ";
  DVLOG(1) << flow << "endpoint channels: "
           << format_ex->Format.nChannels;

  return static_cast<int>(format_ex->Format.nChannels);
}

// static
int WASAPIUnifiedStream::HardwareSampleRate(EDataFlow data_flow) {
  base::win::ScopedCoMem<WAVEFORMATEX> format;
  HRESULT hr = GetMixFormat(data_flow, &format);
  if (FAILED(hr))
    return 0;

  std::string flow = (data_flow == eCapture) ? "[in] " : "[out] ";
  DVLOG(1) << flow << "nSamplesPerSec: " << format->nSamplesPerSec;
  return static_cast<int>(format->nSamplesPerSec);
}

// static
int WASAPIUnifiedStream::HardwareBufferSize(EDataFlow data_flow) {
  int sample_rate = HardwareSampleRate(data_flow);
  if (sample_rate == 0)
    return 0;

  // Number of 100-nanosecond units per second.
  const float kRefTimesPerSec = 10000000.0f;

  // A typical value of |device_period| is 100000 which corresponds to
  // 0.01 seconds or 10 milliseconds. Given a sample rate of 48000 Hz,
  // this device period results in a |buffer_size| of 480 audio frames.
  REFERENCE_TIME device_period = GetAudioEngineDevicePeriod(data_flow);
  int buffer_size = static_cast<int>(
      ((sample_rate * device_period) / kRefTimesPerSec) + 0.5);
  std::string flow = (data_flow == eCapture) ? "[in] " : "[out] ";
  DVLOG(1) << flow << "buffer size: " << buffer_size;

  return buffer_size;
}

void WASAPIUnifiedStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Increase the thread priority.
  audio_io_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  // TODO(henrika): investigate if it is possible to include these additional
  // settings in SetThreadPriority() as well.
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

  bool streaming = true;
  bool error = false;
  HANDLE wait_array[] = { stop_streaming_event_,
                          capture_event_ };

  const int bytes_per_sample = format_.Format.wBitsPerSample >> 3;

  // Keep streaming audio until the stop, or error, event is signaled.
  // The current implementation uses capture events as driving mechanism since
  // extensive testing has shown that it gives us a more reliable callback
  // sequence compared with a scheme where both capture and render events are
  // utilized.
  while (streaming && !error) {
    // Wait for a close-down event, or a new capture event.
    DWORD wait_result = WaitForMultipleObjects(arraysize(wait_array),
                                               wait_array,
                                               FALSE,
                                               INFINITE);
    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_streaming_event_| has been set.
        streaming = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |capture_event_| has been set
        {
          TRACE_EVENT0("audio", "WASAPIUnifiedStream::Run");

          // --- Capture ---

          BYTE* data_ptr = NULL;
          UINT32 num_captured_frames = 0;
          DWORD flags = 0;
          UINT64 device_position = 0;
          UINT64 first_audio_frame_timestamp = 0;

          // Retrieve the amount of data in the capture endpoint buffer.
          hr = audio_capture_client_->GetBuffer(&data_ptr,
                                                &num_captured_frames,
                                                &flags,
                                                &device_position,
                                                &first_audio_frame_timestamp);
          if (FAILED(hr)) {
            DLOG(ERROR) << "Failed to get data from the capture buffer";
            continue;
          }

          if (num_captured_frames != 0) {
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
              // Clear out the capture buffer since silence is reported.
              capture_bus_->Zero();
            } else {
              // Store captured data in an audio bus after de-interleaving
              // the data to match the audio bus structure.
              capture_bus_->FromInterleaved(
                  data_ptr, num_captured_frames, bytes_per_sample);
            }
          }

          hr = audio_capture_client_->ReleaseBuffer(num_captured_frames);
          DLOG_IF(ERROR, FAILED(hr)) << "Failed to release capture buffer";

          // Prepare for rendering by calling OnMoreIOData().
          int frames_filled = source_->OnMoreIOData(
              capture_bus_.get(),
              render_bus_.get(),
              AudioBuffersState(0, 0));
          DCHECK_EQ(frames_filled, render_bus_->frames());

          // --- Render ---

          // Derive the the amount of available space in the endpoint buffer.
          // Avoid render attempt if there is no room for a captured packet.
          UINT32 num_queued_frames = 0;
          audio_output_client_->GetCurrentPadding(&num_queued_frames);
          if (endpoint_render_buffer_size_frames_ - num_queued_frames <
              packet_size_frames_)
            continue;

          // Grab all available space in the rendering endpoint buffer
          // into which the client can write a data packet.
          uint8* audio_data = NULL;
          hr = audio_render_client_->GetBuffer(packet_size_frames_,
                                               &audio_data);
          if (FAILED(hr)) {
            DLOG(ERROR) << "Failed to access render buffer";
            continue;
          }

          // Convert the audio bus content to interleaved integer data using
          // |audio_data| as destination.
          render_bus_->ToInterleaved(
              packet_size_frames_, bytes_per_sample, audio_data);

          // Release the buffer space acquired in the GetBuffer() call.
          audio_render_client_->ReleaseBuffer(packet_size_frames_, 0);
          DLOG_IF(ERROR, FAILED(hr)) << "Failed to release render buffer";
        }
        break;
      default:
        error = true;
        break;
    }
  }

  if (streaming && error) {
    // Stop audio streaming since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    audio_input_client_->Stop();
    audio_output_client_->Stop();
    PLOG(ERROR) << "WASAPI streaming failed.";
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }
}

void WASAPIUnifiedStream::HandleError(HRESULT err) {
  CHECK((started() && GetCurrentThreadId() == audio_io_thread_->tid()) ||
        (!started() && GetCurrentThreadId() == creating_thread_id_));
  NOTREACHED() << "Error code: " << std::hex << err;
  if (source_)
    source_->OnError(this, static_cast<int>(err));
}

void WASAPIUnifiedStream::StopAndJoinThread(HRESULT err) {
  CHECK(GetCurrentThreadId() == creating_thread_id_);
  DCHECK(audio_io_thread_.get());
  SetEvent(stop_streaming_event_.Get());
  audio_io_thread_->Join();
  audio_io_thread_.reset();
  HandleError(err);
}

ScopedComPtr<IAudioRenderClient> WASAPIUnifiedStream::CreateAudioRenderClient(
    IAudioClient* audio_client) {
  ScopedComPtr<IAudioRenderClient> audio_render_client;
  HRESULT hr = S_FALSE;

  // Initialize the audio stream between the client and the device in shared
  // push mode (will not signal an event).
  hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_NOPERSIST,
                                0,
                                0,
                                reinterpret_cast<WAVEFORMATEX*>(&format_),
                                NULL);
  if (FAILED(hr)) {
    LOG(WARNING) << "IAudioClient::Initialize() failed: " << std::hex << hr;
    return audio_render_client;
  }

  // Retrieve the length of the render endpoint buffer shared between the
  // client and the audio engine.
  hr = audio_client->GetBufferSize(&endpoint_render_buffer_size_frames_);
  if (FAILED(hr))
    return audio_render_client;
  DVLOG(1) << "render endpoint buffer size: "
           << endpoint_render_buffer_size_frames_ << " [frames]";

  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  hr = audio_client->GetService(__uuidof(IAudioRenderClient),
                                audio_render_client.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(WARNING) << "IAudioClient::GetService() failed: " << std::hex << hr;
    return audio_render_client;
  }

  return audio_render_client;
}

ScopedComPtr<IAudioCaptureClient>
WASAPIUnifiedStream::CreateAudioCaptureClient(IAudioClient* audio_client) {
  ScopedComPtr<IAudioCaptureClient> audio_capture_client;
  HRESULT hr = S_FALSE;

  // Use event driven audio-buffer processing, i.e, the audio engine will
  // inform us (by signaling an event) when data has been recorded.
  hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                AUDCLNT_STREAMFLAGS_NOPERSIST,
                                0,
                                0,
                                reinterpret_cast<WAVEFORMATEX*>(&format_),
                                NULL);
  if (FAILED(hr))
    return audio_capture_client;

  // Retrieve the length of the capture endpoint buffer shared between the
  // client and the audio engine.
  hr = audio_client->GetBufferSize(&endpoint_capture_buffer_size_frames_);
  if (FAILED(hr))
    return audio_capture_client;
  DVLOG(1) << "capture endpoint buffer size: "
           << endpoint_capture_buffer_size_frames_ << " [frames]";

  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from the capture endpoint buffer.
  hr = audio_client->GetService(__uuidof(IAudioCaptureClient),
                                audio_capture_client.ReceiveVoid());

  return audio_capture_client;
}

}  // namespace media
