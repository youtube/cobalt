// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_input_win.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;

WASAPIAudioInputStream::WASAPIAudioInputStream(
    AudioManagerWin* manager, const AudioParameters& params, ERole device_role)
    : com_init_(ScopedCOMInitializer::kMTA),
      manager_(manager),
      capture_thread_(NULL),
      opened_(false),
      started_(false),
      endpoint_buffer_size_frames_(0),
      device_role_(device_role),
      sink_(NULL) {
  DCHECK(manager_);

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the Avrt.dll";

  // Set up the desired capture format specified by the client.
  format_.nSamplesPerSec = params.sample_rate;
  format_.wFormatTag = WAVE_FORMAT_PCM;
  format_.wBitsPerSample = params.bits_per_sample;
  format_.nChannels = params.channels;
  format_.nBlockAlign = (format_.wBitsPerSample / 8) * format_.nChannels;
  format_.nAvgBytesPerSec = format_.nSamplesPerSec * format_.nBlockAlign;
  format_.cbSize = 0;

  // Size in bytes of each audio frame.
  frame_size_ = format_.nBlockAlign;
  // Store size of audio packets which we expect to get from the audio
  // endpoint device in each capture event.
  packet_size_frames_ = params.GetPacketSize() / format_.nBlockAlign;
  packet_size_bytes_ = params.GetPacketSize();
  DVLOG(1) << "Number of bytes per audio frame  : " << frame_size_;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_ready_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_ready_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_capture_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_capture_event_.IsValid());

  ms_to_frame_count_ = static_cast<double>(params.sample_rate) / 1000.0;

  LARGE_INTEGER performance_frequency;
  if (QueryPerformanceFrequency(&performance_frequency)) {
    perf_count_to_100ns_units_ =
        (10000000.0 / static_cast<double>(performance_frequency.QuadPart));
  } else {
    LOG(ERROR) <<  "High-resolution performance counters are not supported.";
    perf_count_to_100ns_units_ = 0.0;
  }
}

WASAPIAudioInputStream::~WASAPIAudioInputStream() {}

bool WASAPIAudioInputStream::Open() {
  // Verify that we are not already opened.
  if (opened_)
    return false;

  // Obtain a reference to the IMMDevice interface of the default capturing
  // device with the specified role.
  HRESULT hr = SetCaptureDevice(device_role_);
  if (FAILED(hr)) {
    return false;
  }

  // Obtain an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  hr = ActivateCaptureDevice();
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

  opened_ = true;

  return true;
}

void WASAPIAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  DCHECK(opened_);

  if (!opened_)
    return;

  if (started_)
    return;

  sink_ = callback;

  // Create and start the thread that will drive the capturing by waiting for
  // capture events.
  capture_thread_ =
      new base::DelegateSimpleThread(this, "wasapi_capture_thread");
  capture_thread_->Start();

  // Start streaming data between the endpoint buffer and the audio engine.
  HRESULT hr = audio_client_->Start();
  DLOG_IF(ERROR, FAILED(hr)) << "Failed to start input streaming.";

  started_ = SUCCEEDED(hr);
}

void WASAPIAudioInputStream::Stop() {
  if (!started_)
    return;

  // Shut down the capture thread.
  if (stop_capture_event_.IsValid()) {
    SetEvent(stop_capture_event_.Get());
  }

  // Stop the input audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to stop input streaming.";
  }

  // Wait until the thread completes and perform cleanup.
  if (capture_thread_) {
    SetEvent(stop_capture_event_.Get());
    capture_thread_->Join();
    capture_thread_ = NULL;
  }

  started_ = false;
}

void WASAPIAudioInputStream::Close() {
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();
  if (sink_) {
    sink_->OnClose(this);
    sink_ = NULL;
  }

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

// static
double WASAPIAudioInputStream::HardwareSampleRate(ERole device_role) {
  // It is assumed that this static method is called from a COM thread, i.e.,
  // CoInitializeEx() is not called here to avoid STA/MTA conflicts.
  ScopedComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                 NULL,
                                 CLSCTX_INPROC_SERVER,
                                 __uuidof(IMMDeviceEnumerator),
                                 enumerator.ReceiveVoid());
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << hr;
    return 0.0;
  }

  ScopedComPtr<IMMDevice> endpoint_device;
  hr = enumerator->GetDefaultAudioEndpoint(eCapture,
                                           device_role,
                                           endpoint_device.Receive());
  if (FAILED(hr)) {
    // This will happen if there's no audio capture device found or available
    // (e.g. some audio cards that have inputs will still report them as
    // "not found" when no mic is plugged into the input jack).
    LOG(WARNING) << "No audio end point: " << std::hex << hr;
    return 0.0;
  }

  ScopedComPtr<IAudioClient> audio_client;
  hr = endpoint_device->Activate(__uuidof(IAudioClient),
                                 CLSCTX_INPROC_SERVER,
                                 NULL,
                                 audio_client.ReceiveVoid());
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << hr;
    return 0.0;
  }

  base::win::ScopedCoMem<WAVEFORMATEX> audio_engine_mix_format;
  hr = audio_client->GetMixFormat(&audio_engine_mix_format);
  if (FAILED(hr)) {
    NOTREACHED() << "error code: " << hr;
    return 0.0;
  }

  return static_cast<double>(audio_engine_mix_format->nSamplesPerSec);
}

void WASAPIAudioInputStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Increase the thread priority.
  capture_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

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

  // Allocate a buffer with a size that enables us to take care of cases like:
  // 1) The recorded buffer size is smaller, or does not match exactly with,
  //    the selected packet size used in each callback.
  // 2) The selected buffer size is larger than the recorded buffer size in
  //    each event.
  size_t buffer_frame_index = 0;
  size_t capture_buffer_size = std::max(
      2 * endpoint_buffer_size_frames_ * frame_size_,
      2 * packet_size_frames_ * frame_size_);
  scoped_array<uint8> capture_buffer(new uint8[capture_buffer_size]);

  LARGE_INTEGER now_count;
  bool recording = true;
  bool error = false;
  HANDLE wait_array[2] = {stop_capture_event_, audio_samples_ready_event_};

  while (recording && !error) {
    HRESULT hr = S_FALSE;

    // Wait for a close-down event or a new capture event.
    DWORD wait_result = WaitForMultipleObjects(2, wait_array, FALSE, INFINITE);
    switch (wait_result) {
      case WAIT_FAILED:
        error = true;
        break;
      case WAIT_OBJECT_0 + 0:
        // |stop_capture_event_| has been set.
        recording = false;
        break;
      case WAIT_OBJECT_0 + 1:
        {
          // |audio_samples_ready_event_| has been set.
          BYTE* data_ptr = NULL;
          UINT32 num_frames_to_read = 0;
          DWORD flags = 0;
          UINT64 device_position = 0;
          UINT64 first_audio_frame_timestamp = 0;

          // Retrieve the amount of data in the capture endpoint buffer,
          // replace it with silence if required, create callbacks for each
          // packet and store non-delivered data for the next event.
          hr = audio_capture_client_->GetBuffer(&data_ptr,
                                                &num_frames_to_read,
                                                &flags,
                                                &device_position,
                                                &first_audio_frame_timestamp);
          if (FAILED(hr)) {
            DLOG(ERROR) << "Failed to get data from the capture buffer";
            continue;
          }

          if (num_frames_to_read != 0) {
            size_t pos = buffer_frame_index * frame_size_;
            size_t num_bytes = num_frames_to_read * frame_size_;
            DCHECK_GE(capture_buffer_size, pos + num_bytes);

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
              // Clear out the local buffer since silence is reported.
              memset(&capture_buffer[pos], 0, num_bytes);
            } else {
              // Copy captured data from audio engine buffer to local buffer.
              memcpy(&capture_buffer[pos], data_ptr, num_bytes);
            }

            buffer_frame_index += num_frames_to_read;
          }

          hr = audio_capture_client_->ReleaseBuffer(num_frames_to_read);
          DLOG_IF(ERROR, FAILED(hr)) << "Failed to release capture buffer";

          // Derive a delay estimate for the captured audio packet.
          // The value contains two parts (A+B), where A is the delay of the
          // first audio frame in the packet and B is the extra delay
          // contained in any stored data. Unit is in audio frames.
          QueryPerformanceCounter(&now_count);
          double audio_delay_frames =
              ((perf_count_to_100ns_units_ * now_count.QuadPart -
                first_audio_frame_timestamp) / 10000.0) * ms_to_frame_count_ +
                buffer_frame_index - num_frames_to_read;

          // Deliver captured data to the registered consumer using a packet
          // size which was specified at construction.
          uint32 delay_frames = static_cast<uint32>(audio_delay_frames + 0.5);
          while (buffer_frame_index >= packet_size_frames_) {
            uint8* audio_data =
                reinterpret_cast<uint8*>(capture_buffer.get());

            // Deliver data packet and delay estimation to the user.
            sink_->OnData(this,
                          audio_data,
                          packet_size_bytes_,
                          delay_frames * frame_size_);

            // Store parts of the recorded data which can't be delivered
            // using the current packet size. The stored section will be used
            // either in the next while-loop iteration or in the next
            // capture event.
            memmove(&capture_buffer[0],
                    &capture_buffer[packet_size_bytes_],
                    (buffer_frame_index - packet_size_frames_) * frame_size_);

            buffer_frame_index -= packet_size_frames_;
            delay_frames -= packet_size_frames_;
          }
        }
        break;
      default:
        error = true;
        break;
    }
  }

  if (recording && error) {
    // TODO(henrika): perhaps it worth improving the cleanup here by e.g.
    // stopping the audio client, joining the thread etc.?
    NOTREACHED() << "WASAPI capturing failed with error code "
                 << GetLastError();
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }
}

void WASAPIAudioInputStream::HandleError(HRESULT err) {
  NOTREACHED() << "Error code: " << err;
  if (sink_)
    sink_->OnError(this, static_cast<int>(err));
}

HRESULT WASAPIAudioInputStream::SetCaptureDevice(ERole device_role) {
  ScopedComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                 NULL,
                                 CLSCTX_INPROC_SERVER,
                                 __uuidof(IMMDeviceEnumerator),
                                 enumerator.ReceiveVoid());
  if (SUCCEEDED(hr)) {
    // Retrieve the default capture audio endpoint for the specified role.
    // Note that, in Windows Vista, the MMDevice API supports device roles
    // but the system-supplied user interface programs do not.
    hr = enumerator->GetDefaultAudioEndpoint(eCapture,
                                             device_role,
                                             endpoint_device_.Receive());

    // Verify that the audio endpoint device is active. That is, the audio
    // adapter that connects to the endpoint device is present and enabled.
    DWORD state = DEVICE_STATE_DISABLED;
    hr = endpoint_device_->GetState(&state);
    if (SUCCEEDED(hr)) {
      if (!(state & DEVICE_STATE_ACTIVE)) {
        DLOG(ERROR) << "Selected capture device is not active.";
        hr = E_ACCESSDENIED;
      }
    }
  }

  return hr;
}

HRESULT WASAPIAudioInputStream::ActivateCaptureDevice() {
  // Creates and activates an IAudioClient COM object given the selected
  // capture endpoint device.
  HRESULT hr = endpoint_device_->Activate(__uuidof(IAudioClient),
                                          CLSCTX_INPROC_SERVER,
                                          NULL,
                                          audio_client_.ReceiveVoid());
  return hr;
}

HRESULT WASAPIAudioInputStream::GetAudioEngineStreamFormat() {
  // Retrieve the stream format that the audio engine uses for its internal
  // processing/mixing of shared-mode streams.
  return audio_client_->GetMixFormat(&audio_engine_mix_format_);
}

bool WASAPIAudioInputStream::DesiredFormatIsSupported() {
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

HRESULT WASAPIAudioInputStream::InitializeAudioEngine() {
  // Initialize the audio stream between the client and the device.
  // We connect indirectly through the audio engine by using shared mode
  // and WASAPI is initialized in an event driven mode.
  // Note that, |hnsBufferDuration| is set of 0, which ensures that the
  // buffer is never smaller than the minimum buffer size needed to ensure
  // that glitches do not occur between the periodic processing passes.
  // This setting should lead to lowest possible latency.
  HRESULT hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                         AUDCLNT_STREAMFLAGS_NOPERSIST,
                                         0,  // hnsBufferDuration
                                         0,
                                         &format_,
                                         NULL);
  if (FAILED(hr))
    return hr;

  // Retrieve the length of the endpoint buffer shared between the client
  // and the audio engine. The buffer length determines the maximum amount
  // of capture data that the audio engine can read from the endpoint buffer
  // during a single processing pass.
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
  // that an audio application can achieve.
  // TODO(henrika): possibly remove this section when all parts are ready.
  REFERENCE_TIME device_period_shared_mode = 0;
  REFERENCE_TIME device_period_exclusive_mode = 0;
  HRESULT hr_dbg = audio_client_->GetDevicePeriod(
      &device_period_shared_mode, &device_period_exclusive_mode);
  if (SUCCEEDED(hr_dbg)) {
    DVLOG(1) << "device period: "
             << static_cast<double>(device_period_shared_mode / 10000.0)
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
  hr = audio_client_->SetEventHandle(audio_samples_ready_event_.Get());
  if (FAILED(hr))
    return hr;

  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from the capture endpoint buffer.
  hr = audio_client_->GetService(__uuidof(IAudioCaptureClient),
                                 audio_capture_client_.ReceiveVoid());
  return hr;
}
