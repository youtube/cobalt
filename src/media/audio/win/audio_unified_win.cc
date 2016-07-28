// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_unified_win.h"

#include <Functiondiscoverykeys_devpkey.h>

#include "base/debug/trace_event.h"
#include "base/time.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;
using base::win::ScopedCoMem;

// Time in milliseconds between two successive delay measurements.
// We save resources by not updating the delay estimates for each capture
// event (typically 100Hz rate).
static const size_t kTimeDiffInMillisecondsBetweenDelayMeasurements = 1000;

// Compare two sets of audio parameters and return true if they are equal.
// Note that bits_per_sample() is excluded from this comparison since Core
// Audio can deal with most bit depths. As an example, if the native/mixing
// bit depth is 32 bits (default), opening at 16 or 24 still works fine and
// the audio engine will do the required conversion for us.
static bool CompareAudioParameters(const media::AudioParameters& a,
                                   const media::AudioParameters& b) {
  return (a.format() == b.format() &&
          a.channels() == b.channels() &&
          a.sample_rate() == b.sample_rate() &&
          a.frames_per_buffer() == b.frames_per_buffer());
}

// Use the acquired IAudioClock interface to derive a time stamp of the audio
// sample which is currently playing through the speakers.
static double SpeakerStreamPosInMilliseconds(IAudioClock* clock) {
  UINT64 device_frequency = 0, position = 0;
  if (FAILED(clock->GetFrequency(&device_frequency)) ||
      FAILED(clock->GetPosition(&position, NULL))) {
    return 0.0;
  }

  return base::Time::kMillisecondsPerSecond *
      (static_cast<double>(position) / device_frequency);
}

// Get a time stamp in milliseconds given number of audio frames in |num_frames|
// using the current sample rate |fs| as scale factor.
// Example: |num_frames| = 960 and |fs| = 48000 => 20 [ms].
static double CurrentStreamPosInMilliseconds(UINT64 num_frames, DWORD fs) {
  return base::Time::kMillisecondsPerSecond *
      (static_cast<double>(num_frames) / fs);
}

// Convert a timestamp in milliseconds to byte units given the audio format
// in |format|.
// Example: |ts_milliseconds| equals 10, sample rate is 48000 and frame size
// is 4 bytes per audio frame => 480 * 4 = 1920 [bytes].
static int MillisecondsToBytes(double ts_milliseconds,
                               const WAVEFORMATPCMEX& format) {
  double seconds = ts_milliseconds / base::Time::kMillisecondsPerSecond;
  return static_cast<int>(seconds * format.Format.nSamplesPerSec *
      format.Format.nBlockAlign + 0.5);
}

namespace media {

WASAPIUnifiedStream::WASAPIUnifiedStream(AudioManagerWin* manager,
                                         const AudioParameters& params)
    : creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      share_mode_(CoreAudioUtil::GetShareMode()),
      audio_io_thread_(NULL),
      opened_(false),
      endpoint_render_buffer_size_frames_(0),
      endpoint_capture_buffer_size_frames_(0),
      num_written_frames_(0),
      total_delay_ms_(0.0),
      source_(NULL),
      capture_bus_(AudioBus::Create(params)),
      render_bus_(AudioBus::Create(params)) {
  DCHECK(manager_);

  DVLOG_IF(1, !HasUnifiedDefaultIO()) << "Unified audio I/O is not supported.";
  DVLOG_IF(1, share_mode_ == AUDCLNT_SHAREMODE_EXCLUSIVE)
      << "Core Audio (WASAPI) EXCLUSIVE MODE is enabled.";

#if !defined(NDEBUG)
  // Add log message if input parameters are not identical to the preferred
  // parameters.
  AudioParameters mix_params;
  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
      eRender, eConsole, &mix_params);
  DVLOG_IF(1, SUCCEEDED(hr) && !CompareAudioParameters(params, mix_params)) <<
      "Input and preferred parameters are not identical.";
#endif

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

  ScopedComPtr<IAudioClient> audio_output_client =
      CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  if (!audio_output_client)
    return false;

  if (!CoreAudioUtil::IsFormatSupported(audio_output_client,
                                        share_mode_,
                                        &format_)) {
    return false;
  }

  HRESULT hr = S_FALSE;
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    hr = CoreAudioUtil::SharedModeInitialize(
        audio_output_client, &format_, NULL,
        &endpoint_render_buffer_size_frames_);
  } else {
    // TODO(henrika): add support for AUDCLNT_SHAREMODE_EXCLUSIVE.
  }
  if (FAILED(hr))
    return false;

  ScopedComPtr<IAudioRenderClient> audio_render_client =
      CoreAudioUtil::CreateRenderClient(audio_output_client);
  if (!audio_render_client)
    return false;

  // Capture side:

  ScopedComPtr<IAudioClient> audio_input_client =
      CoreAudioUtil::CreateDefaultClient(eCapture, eConsole);
  if (!audio_input_client)
    return false;

  if (!CoreAudioUtil::IsFormatSupported(audio_input_client,
                                        share_mode_,
                                        &format_)) {
    return false;
  }

  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // Include valid event handle for event-driven initialization.
    hr = CoreAudioUtil::SharedModeInitialize(
        audio_input_client, &format_, capture_event_.Get(),
        &endpoint_capture_buffer_size_frames_);
  } else {
    // TODO(henrika): add support for AUDCLNT_SHAREMODE_EXCLUSIVE.
  }
  if (FAILED(hr))
    return false;

  ScopedComPtr<IAudioCaptureClient> audio_capture_client =
      CoreAudioUtil::CreateCaptureClient(audio_input_client);
  if (!audio_capture_client)
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
  HRESULT hr = audio_input_client_->Start();
  if (FAILED(hr)) {
    StopAndJoinThread(hr);
    return;
  }

  // Reset the counter for number of rendered frames taking into account the
  // fact that we always initialize the render side with silence.
  UINT32 num_queued_frames = 0;
  audio_output_client_->GetCurrentPadding(&num_queued_frames);
  DCHECK_EQ(num_queued_frames, endpoint_render_buffer_size_frames_);
  num_written_frames_ = num_queued_frames;

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
  // TODO(henrika): this check is is only needed for shared-mode streams.
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
  AudioParameters in_params;
  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(eCapture, eConsole,
                                                          &in_params);
  if (FAILED(hr))
    return false;

  AudioParameters out_params;
  hr = CoreAudioUtil::GetPreferredAudioParameters(eRender, eConsole,
                                                  &out_params);
  if (FAILED(hr))
    return false;

  return CompareAudioParameters(in_params, out_params);
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

  // The IAudioClock interface enables us to monitor a stream's data
  // rate and the current position in the stream. Allocate it before we
  // start spinning.
  ScopedComPtr<IAudioClock> audio_output_clock;
  HRESULT hr = audio_output_client_->GetService(
      __uuidof(IAudioClock), audio_output_clock.ReceiveVoid());
  LOG_IF(WARNING, FAILED(hr)) << "Failed to create IAudioClock: "
                              << std::hex << hr;

  // Stores a delay measurement (unit is in bytes). This variable is not
  // updated at each event, but the update frequency is set by a constant
  // called |kTimeDiffInMillisecondsBetweenDelayMeasurements|.
  int total_delay_bytes = 0;

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
          UINT64 capture_time_stamp = 0;

          base::TimeTicks now_tick = base::TimeTicks::HighResNow();

          // Retrieve the amount of data in the capture endpoint buffer.
          // |endpoint_capture_time_stamp| is the value of the performance
          // counter at the time that the audio endpoint device recorded
          // the device position of the first audio frame in the data packet.
          hr = audio_capture_client_->GetBuffer(&data_ptr,
                                                &num_captured_frames,
                                                &flags,
                                                &device_position,
                                                &capture_time_stamp);
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

          // Save resource by not asking for new delay estimates each time.
          // These estimates are fairly stable and it is perfectly safe to only
          // sample at a rate of ~1Hz.
          // TODO(henrika): it might be possible to use a fixed delay instead.
          if ((now_tick - last_delay_sample_time_).InMilliseconds() >
              kTimeDiffInMillisecondsBetweenDelayMeasurements) {
            // Calculate the estimated capture delay, i.e., the latency between
            // the recording time and the time we when we are notified about
            // the recorded data. Note that the capture time stamp is given in
            // 100-nanosecond (0.1 microseconds) units.
            base::TimeDelta diff = now_tick -
                base::TimeTicks::FromInternalValue(0.1 * capture_time_stamp);
            const double capture_delay_ms = diff.InMillisecondsF();

            // Calculate the estimated render delay, i.e., the time difference
            // between the time when data is added to the endpoint buffer and
            // when the data is played out on the actual speaker.
            const double stream_pos = CurrentStreamPosInMilliseconds(
                num_written_frames_ + packet_size_frames_,
                format_.Format.nSamplesPerSec);
            const double speaker_pos =
                SpeakerStreamPosInMilliseconds(audio_output_clock);
            const double render_delay_ms = stream_pos - speaker_pos;

            // Derive the total delay, i.e., the sum of the input and output
            // delays. Also convert the value into byte units.
            total_delay_ms_ = capture_delay_ms + render_delay_ms;
            last_delay_sample_time_ = now_tick;
            DVLOG(3) << "total_delay_ms  : " << total_delay_ms_;
            total_delay_bytes = MillisecondsToBytes(total_delay_ms_, format_);
          }

          // Prepare for rendering by calling OnMoreIOData().
          int frames_filled = source_->OnMoreIOData(
              capture_bus_.get(),
              render_bus_.get(),
              AudioBuffersState(0, total_delay_bytes));
          DCHECK_EQ(frames_filled, render_bus_->frames());

          // --- Render ---

          // Keep track of number of rendered frames since we need it for
          // our delay calculations.
          num_written_frames_ += frames_filled;

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

}  // namespace media
