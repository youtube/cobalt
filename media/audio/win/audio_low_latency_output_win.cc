// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_output_win.h"

#include <Functiondiscoverykeys_devpkey.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;
using base::win::ScopedCoMem;

namespace media {

typedef uint32 ChannelConfig;

// Retrieves the stream format that the audio engine uses for its internal
// processing/mixing of shared-mode streams.
static HRESULT GetMixFormat(ERole device_role, WAVEFORMATEX** device_format) {
  // Note that we are using the IAudioClient::GetMixFormat() API to get the
  // device format in this function. It is in fact possible to be "more native",
  // and ask the endpoint device directly for its properties. Given a reference
  // to the IMMDevice interface of an endpoint object, a client can obtain a
  // reference to the endpoint object's property store by calling the
  // IMMDevice::OpenPropertyStore() method. However, I have not been able to
  // access any valuable information using this method on my HP Z600 desktop,
  // hence it feels more appropriate to use the IAudioClient::GetMixFormat()
  // approach instead.

  // Calling this function only makes sense for shared mode streams, since
  // if the device will be opened in exclusive mode, then the application
  // specified format is used instead. However, the result of this method can
  // be useful for testing purposes so we don't DCHECK here.
  DLOG_IF(WARNING, WASAPIAudioOutputStream::GetShareMode() ==
          AUDCLNT_SHAREMODE_EXCLUSIVE) <<
      "The mixing sample rate will be ignored for exclusive-mode streams.";

  // It is assumed that this static method is called from a COM thread, i.e.,
  // CoInitializeEx() is not called here again to avoid STA/MTA conflicts.
  ScopedComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                enumerator.ReceiveVoid());
  if (FAILED(hr))
    return hr;

  ScopedComPtr<IMMDevice> endpoint_device;
  hr = enumerator->GetDefaultAudioEndpoint(eRender,
                                           device_role,
                                           endpoint_device.Receive());
  if (FAILED(hr))
    return hr;

  ScopedComPtr<IAudioClient> audio_client;
  hr = endpoint_device->Activate(__uuidof(IAudioClient),
                                 CLSCTX_INPROC_SERVER,
                                 NULL,
                                 audio_client.ReceiveVoid());
  return SUCCEEDED(hr) ? audio_client->GetMixFormat(device_format) : hr;
}

// Retrieves an integer mask which corresponds to the channel layout the
// audio engine uses for its internal processing/mixing of shared-mode
// streams. This mask indicates which channels are present in the multi-
// channel stream. The least significant bit corresponds with the Front Left
// speaker, the next least significant bit corresponds to the Front Right
// speaker, and so on, continuing in the order defined in KsMedia.h.
// See http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
// for more details.
static ChannelConfig GetChannelConfig() {
  // Use a WAVEFORMATEXTENSIBLE structure since it can specify both the
  // number of channels and the mapping of channels to speakers for
  // multichannel devices.
  base::win::ScopedCoMem<WAVEFORMATPCMEX> format_ex;
  HRESULT hr = S_FALSE;
  hr = GetMixFormat(eConsole, reinterpret_cast<WAVEFORMATEX**>(&format_ex));
  if (FAILED(hr))
    return 0;

  // The dwChannelMask member specifies which channels are present in the
  // multichannel stream. The least significant bit corresponds to the
  // front left speaker, the next least significant bit corresponds to the
  // front right speaker, and so on.
  // See http://msdn.microsoft.com/en-us/library/windows/desktop/dd757714(v=vs.85).aspx
  // for more details on the channel mapping.
  DVLOG(2) << "dwChannelMask: 0x" << std::hex << format_ex->dwChannelMask;

#if !defined(NDEBUG)
  // See http://en.wikipedia.org/wiki/Surround_sound for more details on
  // how to name various speaker configurations. The list below is not complete.
  const char* speaker_config = "Undefined";
  switch (format_ex->dwChannelMask) {
    case KSAUDIO_SPEAKER_MONO:
      speaker_config = "Mono";
      break;
    case KSAUDIO_SPEAKER_STEREO:
      speaker_config = "Stereo";
      break;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      speaker_config = "5.1 surround";
      break;
    case KSAUDIO_SPEAKER_5POINT1:
      speaker_config = "5.1";
      break;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      speaker_config = "7.1 surround";
      break;
    case KSAUDIO_SPEAKER_7POINT1:
      speaker_config = "7.1";
      break;
    default:
      break;
  }
  DVLOG(2) << "speaker configuration: " << speaker_config;
#endif

  return static_cast<ChannelConfig>(format_ex->dwChannelMask);
}

// Converts Microsoft's channel configuration to ChannelLayout.
// This mapping is not perfect but the best we can do given the current
// ChannelLayout enumerator and the Windows-specific speaker configurations
// defined in ksmedia.h. Don't assume that the channel ordering in
// ChannelLayout is exactly the same as the Windows specific configuration.
// As an example: KSAUDIO_SPEAKER_7POINT1_SURROUND is mapped to
// CHANNEL_LAYOUT_7_1 but the positions of Back L, Back R and Side L, Side R
// speakers are different in these two definitions.
static ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config) {
  switch (config) {
    case KSAUDIO_SPEAKER_DIRECTOUT:
      return CHANNEL_LAYOUT_NONE;
    case KSAUDIO_SPEAKER_MONO:
      return CHANNEL_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_SURROUND:
      return CHANNEL_LAYOUT_4_0;
    case KSAUDIO_SPEAKER_5POINT1:
      return CHANNEL_LAYOUT_5_1_BACK;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      return CHANNEL_LAYOUT_5_1;
    case KSAUDIO_SPEAKER_7POINT1:
      return CHANNEL_LAYOUT_7_1_WIDE;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      return CHANNEL_LAYOUT_7_1;
    default:
      DVLOG(1) << "Unsupported channel layout: " << config;
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

// static
AUDCLNT_SHAREMODE WASAPIAudioOutputStream::GetShareMode() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio))
    return AUDCLNT_SHAREMODE_EXCLUSIVE;
  return AUDCLNT_SHAREMODE_SHARED;
}

WASAPIAudioOutputStream::WASAPIAudioOutputStream(AudioManagerWin* manager,
                                                 const AudioParameters& params,
                                                 ERole device_role)
    : creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      opened_(false),
      restart_rendering_mode_(false),
      volume_(1.0),
      endpoint_buffer_size_frames_(0),
      device_role_(device_role),
      share_mode_(GetShareMode()),
      client_channel_count_(params.channels()),
      num_written_frames_(0),
      source_(NULL),
      audio_bus_(AudioBus::Create(params)) {
  DCHECK(manager_);

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the avrt.dll";

  if (share_mode_ == AUDCLNT_SHAREMODE_EXCLUSIVE) {
    VLOG(1) << ">> Note that EXCLUSIVE MODE is enabled <<";
  }

  // Set up the desired render format specified by the client. We use the
  // WAVE_FORMAT_EXTENSIBLE structure to ensure that multiple channel ordering
  // and high precision data can be supported.

  // Begin with the WAVEFORMATEX structure that specifies the basic format.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = client_channel_count_;
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = params.bits_per_sample();
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE.
  format_.Samples.wValidBitsPerSample = params.bits_per_sample();
  format_.dwChannelMask = GetChannelConfig();
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  // Size in bytes of each audio frame.
  frame_size_ = format->nBlockAlign;

  // Store size (in different units) of audio packets which we expect to
  // get from the audio endpoint device in each render event.
  packet_size_frames_ = params.GetBytesPerBuffer() / format->nBlockAlign;
  packet_size_bytes_ = params.GetBytesPerBuffer();
  packet_size_ms_ = (1000.0 * packet_size_frames_) / params.sample_rate();
  DVLOG(1) << "Number of bytes per audio frame  : " << frame_size_;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;
  DVLOG(1) << "Number of bytes per packet       : " << packet_size_bytes_;
  DVLOG(1) << "Number of milliseconds per packet: " << packet_size_ms_;

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_render_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_render_event_.IsValid());
}

WASAPIAudioOutputStream::~WASAPIAudioOutputStream() {}

bool WASAPIAudioOutputStream::Open() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (opened_)
    return true;

  // Channel mixing is not supported, it must be handled by ChannelMixer.
  if (format_.Format.nChannels != client_channel_count_) {
    LOG(ERROR) << "Channel down-mixing is not supported.";
    return false;
  }

  // Create an IMMDeviceEnumerator interface and obtain a reference to
  // the IMMDevice interface of the default rendering device with the
  // specified role.
  HRESULT hr = SetRenderDevice();
  if (FAILED(hr)) {
    return false;
  }

  // Obtain an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  hr = ActivateRenderDevice();
  if (FAILED(hr)) {
    return false;
  }

  // Verify that the selected audio endpoint supports the specified format
  // set during construction.
  // In exclusive mode, the client can choose to open the stream in any audio
  // format that the endpoint device supports. In shared mode, the client must
  // open the stream in the mix format that is currently in use by the audio
  // engine (or a format that is similar to the mix format). The audio engine's
  // input streams and the output mix from the engine are all in this format.
  if (!DesiredFormatIsSupported()) {
    return false;
  }

  // Initialize the audio stream between the client and the device using
  // shared or exclusive mode and a lowest possible glitch-free latency.
  // We will enter different code paths depending on the specified share mode.
  hr = InitializeAudioEngine();
  if (FAILED(hr)) {
    return false;
  }

  opened_ = true;
  return true;
}

void WASAPIAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  CHECK(callback);
  CHECK(opened_);

  if (render_thread_.get()) {
    CHECK_EQ(callback, source_);
    return;
  }

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
  render_thread_.reset(
      new base::DelegateSimpleThread(this, "wasapi_render_thread"));
  render_thread_->Start();

  // Start streaming data between the endpoint buffer and the audio engine.
  hr = audio_client_->Start();
  if (FAILED(hr)) {
    SetEvent(stop_render_event_.Get());
    render_thread_->Join();
    render_thread_.reset();
    HandleError(hr);
  }
}

void WASAPIAudioOutputStream::Stop() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (!render_thread_.get())
    return;

  // Stop output audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to stop output streaming: " << std::hex << hr;
  }

  // Wait until the thread completes and perform cleanup.
  SetEvent(stop_render_event_.Get());
  render_thread_->Join();
  render_thread_.reset();

  // Ensure that we don't quit the main thread loop immediately next
  // time Start() is called.
  ResetEvent(stop_render_event_.Get());

  // Clear source callback, it'll be set again on the next Start() call.
  source_ = NULL;

  // Flush all pending data and reset the audio clock stream position to 0.
  hr = audio_client_->Reset();
  if (FAILED(hr)) {
    DLOG_IF(ERROR, hr != AUDCLNT_E_NOT_INITIALIZED)
        << "Failed to reset streaming: " << std::hex << hr;
  }

  // Extra safety check to ensure that the buffers are cleared.
  // If the buffers are not cleared correctly, the next call to Start()
  // would fail with AUDCLNT_E_BUFFER_ERROR at IAudioRenderClient::GetBuffer().
  // This check is is only needed for shared-mode streams.
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    UINT32 num_queued_frames = 0;
    audio_client_->GetCurrentPadding(&num_queued_frames);
    DCHECK_EQ(0u, num_queued_frames);
  }
}

void WASAPIAudioOutputStream::Close() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

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
int WASAPIAudioOutputStream::HardwareChannelCount() {
  // Use a WAVEFORMATEXTENSIBLE structure since it can specify both the
  // number of channels and the mapping of channels to speakers for
  // multichannel devices.
  base::win::ScopedCoMem<WAVEFORMATPCMEX> format_ex;
  HRESULT hr = GetMixFormat(
      eConsole, reinterpret_cast<WAVEFORMATEX**>(&format_ex));
  if (FAILED(hr))
    return 0;

  // Number of channels in the stream. Corresponds to the number of bits
  // set in the dwChannelMask.
  DVLOG(1) << "endpoint channels (out): " << format_ex->Format.nChannels;

  return static_cast<int>(format_ex->Format.nChannels);
}

// static
ChannelLayout WASAPIAudioOutputStream::HardwareChannelLayout() {
  return ChannelConfigToChannelLayout(GetChannelConfig());
}

// static
int WASAPIAudioOutputStream::HardwareSampleRate(ERole device_role) {
  base::win::ScopedCoMem<WAVEFORMATEX> format;
  HRESULT hr = GetMixFormat(device_role, &format);
  if (FAILED(hr))
    return 0;

  DVLOG(2) << "nSamplesPerSec: " << format->nSamplesPerSec;
  return static_cast<int>(format->nSamplesPerSec);
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
        {
          // |audio_samples_render_event_| has been set.
          UINT32 num_queued_frames = 0;
          uint8* audio_data = NULL;

          // Contains how much new data we can write to the buffer without
          // the risk of overwriting previously written data that the audio
          // engine has not yet read from the buffer.
          size_t num_available_frames = 0;

          if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
            // Get the padding value which represents the amount of rendering
            // data that is queued up to play in the endpoint buffer.
            hr = audio_client_->GetCurrentPadding(&num_queued_frames);
            num_available_frames =
                endpoint_buffer_size_frames_ - num_queued_frames;
          } else {
            // While the stream is running, the system alternately sends one
            // buffer or the other to the client. This form of double buffering
            // is referred to as "ping-ponging". Each time the client receives
            // a buffer from the system (triggers this event) the client must
            // process the entire buffer. Calls to the GetCurrentPadding method
            // are unnecessary because the packet size must always equal the
            // buffer size. In contrast to the shared mode buffering scheme,
            // the latency for an event-driven, exclusive-mode stream depends
            // directly on the buffer size.
            num_available_frames = endpoint_buffer_size_frames_;
          }

          // Check if there is enough available space to fit the packet size
          // specified by the client.
          if (FAILED(hr) || (num_available_frames < packet_size_frames_))
            continue;

          // Derive the number of packets we need get from the client to
          // fill up the available area in the endpoint buffer.
          // |num_packets| will always be one for exclusive-mode streams.
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
              double pos_sample_playing_frames = format_.Format.nSamplesPerSec *
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

            uint32 num_filled_bytes = 0;
            const int bytes_per_sample = format_.Format.wBitsPerSample >> 3;

            int frames_filled = source_->OnMoreData(
                audio_bus_.get(), AudioBuffersState(0, audio_delay_bytes));
            num_filled_bytes = frames_filled * frame_size_;
            DCHECK_LE(num_filled_bytes, packet_size_bytes_);
            // Note: If this ever changes to output raw float the data must be
            // clipped and sanitized since it may come from an untrusted
            // source such as NaCl.
            audio_bus_->ToInterleaved(
                frames_filled, bytes_per_sample, audio_data);

            // Perform in-place, software-volume adjustments.
            media::AdjustVolume(audio_data,
                                num_filled_bytes,
                                audio_bus_->channels(),
                                bytes_per_sample,
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
  CHECK((started() && GetCurrentThreadId() == render_thread_->tid()) ||
        (!started() && GetCurrentThreadId() == creating_thread_id_));
  NOTREACHED() << "Error code: " << std::hex << err;
  if (source_)
    source_->OnError(this, static_cast<int>(err));
}

HRESULT WASAPIAudioOutputStream::SetRenderDevice() {
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
    // Note that, in Windows Vista, the MMDevice API supports device roles
    // but the system-supplied user interface programs do not.
    hr = device_enumerator->GetDefaultAudioEndpoint(
        eRender, device_role_, endpoint_device.Receive());
    if (FAILED(hr))
      return hr;

    // Verify that the audio endpoint device is active. That is, the audio
    // adapter that connects to the endpoint device is present and enabled.
    DWORD state = DEVICE_STATE_DISABLED;
    hr = endpoint_device->GetState(&state);
    if (SUCCEEDED(hr)) {
      if (!(state & DEVICE_STATE_ACTIVE)) {
        DLOG(ERROR) << "Selected render device is not active.";
        hr = E_ACCESSDENIED;
      }
    }
  }

  if (SUCCEEDED(hr)) {
    device_enumerator_ = device_enumerator;
    endpoint_device_ = endpoint_device;
  }

  return hr;
}

HRESULT WASAPIAudioOutputStream::ActivateRenderDevice() {
  ScopedComPtr<IAudioClient> audio_client;

  // Creates and activates an IAudioClient COM object given the selected
  // render endpoint device.
  HRESULT hr = endpoint_device_->Activate(__uuidof(IAudioClient),
                                          CLSCTX_INPROC_SERVER,
                                          NULL,
                                          audio_client.ReceiveVoid());
  if (SUCCEEDED(hr)) {
    // Retrieve the stream format that the audio engine uses for its internal
    // processing/mixing of shared-mode streams.
    audio_engine_mix_format_.Reset(NULL);
    hr = audio_client->GetMixFormat(
        reinterpret_cast<WAVEFORMATEX**>(&audio_engine_mix_format_));

    if (SUCCEEDED(hr)) {
      audio_client_ = audio_client;
    }
  }

  return hr;
}

bool WASAPIAudioOutputStream::DesiredFormatIsSupported() {
  // Determine, before calling IAudioClient::Initialize(), whether the audio
  // engine supports a particular stream format.
  // In shared mode, the audio engine always supports the mix format,
  // which is stored in the |audio_engine_mix_format_| member and it is also
  // possible to receive a proposed (closest) format if the current format is
  // not supported.
  base::win::ScopedCoMem<WAVEFORMATEXTENSIBLE> closest_match;
  HRESULT hr = audio_client_->IsFormatSupported(
      share_mode_, reinterpret_cast<WAVEFORMATEX*>(&format_),
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

HRESULT WASAPIAudioOutputStream::InitializeAudioEngine() {
#if !defined(NDEBUG)
  // The period between processing passes by the audio engine is fixed for a
  // particular audio endpoint device and represents the smallest processing
  // quantum for the audio engine. This period plus the stream latency between
  // the buffer and endpoint device represents the minimum possible latency
  // that an audio application can achieve in shared mode.
  {
    REFERENCE_TIME default_device_period = 0;
    REFERENCE_TIME minimum_device_period = 0;
    HRESULT hr_dbg = audio_client_->GetDevicePeriod(&default_device_period,
      &minimum_device_period);
    if (SUCCEEDED(hr_dbg)) {
      // Shared mode device period.
      DVLOG(1) << "shared mode (default) device period: "
               << static_cast<double>(default_device_period / 10000.0)
               << " [ms]";
      // Exclusive mode device period.
      DVLOG(1) << "exclusive mode (minimum) device period: "
               << static_cast<double>(minimum_device_period / 10000.0)
               << " [ms]";
    }

    REFERENCE_TIME latency = 0;
    hr_dbg = audio_client_->GetStreamLatency(&latency);
    if (SUCCEEDED(hr_dbg)) {
      DVLOG(1) << "stream latency: " << static_cast<double>(latency / 10000.0)
               << " [ms]";
    }
  }
#endif

  HRESULT hr = S_FALSE;

  // Perform different initialization depending on if the device shall be
  // opened in shared mode or in exclusive mode.
  hr = (share_mode_ == AUDCLNT_SHAREMODE_SHARED) ?
      SharedModeInitialization() : ExclusiveModeInitialization();
  if (FAILED(hr)) {
    LOG(WARNING) << "IAudioClient::Initialize() failed: " << std::hex << hr;
    return hr;
  }

  // Retrieve the length of the endpoint buffer. The buffer length represents
  // the maximum amount of rendering data that the client can write to
  // the endpoint buffer during a single processing pass.
  // A typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
  hr = audio_client_->GetBufferSize(&endpoint_buffer_size_frames_);
  if (FAILED(hr))
    return hr;
  DVLOG(1) << "endpoint buffer size: " << endpoint_buffer_size_frames_
           << " [frames]";

  // The buffer scheme for exclusive mode streams is not designed for max
  // flexibility. We only allow a "perfect match" between the packet size set
  // by the user and the actual endpoint buffer size.
  if (share_mode_ == AUDCLNT_SHAREMODE_EXCLUSIVE &&
      endpoint_buffer_size_frames_ != packet_size_frames_) {
    hr = AUDCLNT_E_INVALID_SIZE;
    DLOG(ERROR) << "AUDCLNT_E_INVALID_SIZE";
    return hr;
  }

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

HRESULT WASAPIAudioOutputStream::SharedModeInitialization() {
  DCHECK_EQ(share_mode_, AUDCLNT_SHAREMODE_SHARED);

  // TODO(henrika): this buffer scheme is still under development.
  // The exact details are yet to be determined based on tests with different
  // audio clients.
  int glitch_free_buffer_size_ms = static_cast<int>(packet_size_ms_ + 0.5);
  if (audio_engine_mix_format_->Format.nSamplesPerSec % 8000 == 0) {
    // Initial tests have shown that we have to add 10 ms extra to
    // ensure that we don't run empty for any packet size.
    glitch_free_buffer_size_ms += 10;
  } else if (audio_engine_mix_format_->Format.nSamplesPerSec % 11025 == 0) {
    // Initial tests have shown that we have to add 20 ms extra to
    // ensure that we don't run empty for any packet size.
    glitch_free_buffer_size_ms += 20;
  } else {
    DLOG(WARNING) << "Unsupported sample rate "
        << audio_engine_mix_format_->Format.nSamplesPerSec << " detected";
    glitch_free_buffer_size_ms += 20;
  }
  DVLOG(1) << "glitch_free_buffer_size_ms: " << glitch_free_buffer_size_ms;
  REFERENCE_TIME requested_buffer_duration =
      static_cast<REFERENCE_TIME>(glitch_free_buffer_size_ms * 10000);

  // Initialize the audio stream between the client and the device.
  // We connect indirectly through the audio engine by using shared mode
  // and WASAPI is initialized in an event driven mode.
  // Note that this API ensures that the buffer is never smaller than the
  // minimum buffer size needed to ensure glitch-free rendering.
  // If we requests a buffer size that is smaller than the audio engine's
  // minimum required buffer size, the method sets the buffer size to this
  // minimum buffer size rather than to the buffer size requested.
  HRESULT hr = S_FALSE;
  hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                 AUDCLNT_STREAMFLAGS_NOPERSIST,
                                 requested_buffer_duration,
                                 0,
                                 reinterpret_cast<WAVEFORMATEX*>(&format_),
                                 NULL);
  return hr;
}

HRESULT WASAPIAudioOutputStream::ExclusiveModeInitialization() {
  DCHECK_EQ(share_mode_, AUDCLNT_SHAREMODE_EXCLUSIVE);

  float f = (1000.0 * packet_size_frames_) / format_.Format.nSamplesPerSec;
  REFERENCE_TIME requested_buffer_duration =
      static_cast<REFERENCE_TIME>(f * 10000.0 + 0.5);

  // Initialize the audio stream between the client and the device.
  // For an exclusive-mode stream that uses event-driven buffering, the
  // caller must specify nonzero values for hnsPeriodicity and
  // hnsBufferDuration, and the values of these two parameters must be equal.
  // The Initialize method allocates two buffers for the stream. Each buffer
  // is equal in duration to the value of the hnsBufferDuration parameter.
  // Following the Initialize call for a rendering stream, the caller should
  // fill the first of the two buffers before starting the stream.
  HRESULT hr = S_FALSE;
  hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                 AUDCLNT_STREAMFLAGS_NOPERSIST,
                                 requested_buffer_duration,
                                 requested_buffer_duration,
                                 reinterpret_cast<WAVEFORMATEX*>(&format_),
                                 NULL);
  if (FAILED(hr)) {
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
      LOG(ERROR) << "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";

      UINT32 aligned_buffer_size = 0;
      audio_client_->GetBufferSize(&aligned_buffer_size);
      DVLOG(1) << "Use aligned buffer size instead: " << aligned_buffer_size;
      audio_client_.Release();

      // Calculate new aligned periodicity. Each unit of reference time
      // is 100 nanoseconds.
      REFERENCE_TIME aligned_buffer_duration = static_cast<REFERENCE_TIME>(
          (10000000.0 * aligned_buffer_size / format_.Format.nSamplesPerSec)
          + 0.5);

      // It is possible to re-activate and re-initialize the audio client
      // at this stage but we bail out with an error code instead and
      // combine it with a log message which informs about the suggested
      // aligned buffer size which should be used instead.
      DVLOG(1) << "aligned_buffer_duration: "
                << static_cast<double>(aligned_buffer_duration / 10000.0)
                << " [ms]";
    } else if (hr == AUDCLNT_E_INVALID_DEVICE_PERIOD) {
      // We will get this error if we try to use a smaller buffer size than
      // the minimum supported size (usually ~3ms on Windows 7).
      LOG(ERROR) << "AUDCLNT_E_INVALID_DEVICE_PERIOD";
    }
  }

  return hr;
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

}  // namespace media
