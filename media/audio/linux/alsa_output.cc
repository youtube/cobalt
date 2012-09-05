// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THREAD SAFETY
//
// AlsaPcmOutputStream object is *not* thread-safe and should only be used
// from the audio thread.  We DCHECK on this assumption whenever we can.
//
// SEMANTICS OF Close()
//
// Close() is responsible for cleaning up any resources that were acquired after
// a successful Open().  Close() will nullify any scheduled outstanding runnable
// methods.
//
//
// SEMANTICS OF ERROR STATES
//
// The object has two distinct error states: |state_| == kInError
// and |stop_stream_|.  The |stop_stream_| variable is used to indicate
// that the playback_handle should no longer be used either because of a
// hardware/low-level event.
//
// When |state_| == kInError, all public API functions will fail with an error
// (Start() will call the OnError() function on the callback immediately), or
// no-op themselves with the exception of Close().  Even if an error state has
// been entered, if Open() has previously returned successfully, Close() must be
// called to cleanup the ALSA devices and release resources.
//
// When |stop_stream_| is set, no more commands will be made against the
// ALSA device, and playback will effectively stop.  From the client's point of
// view, it will seem that the device has just clogged and stopped requesting
// data.

#include "media/audio/linux/alsa_output.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "media/audio/audio_util.h"
#include "media/audio/linux/alsa_util.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/audio/linux/audio_manager_linux.h"
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"

namespace media {

// Amount of time to wait if we've exhausted the data source.  This is to avoid
// busy looping.
static const uint32 kNoDataSleepMilliseconds = 10;

// Mininum interval between OnMoreData() calls.  This is to avoid glitches for
// WebAudio which needs time to generate new data.
static const uint32 kMinIntervalBetweenOnMoreDataCallsInMs = 5;

// According to the linux nanosleep manpage, nanosleep on linux can miss the
// deadline by up to 10ms because the kernel timeslice is 10ms.  This should be
// enough to compensate for the timeslice, and any additional slowdowns.
static const uint32 kSleepErrorMilliseconds = 10;

// Set to 0 during debugging if you want error messages due to underrun
// events or other recoverable errors.
#if defined(NDEBUG)
static const int kPcmRecoverIsSilent = 1;
#else
static const int kPcmRecoverIsSilent = 0;
#endif

// ALSA is currently limited to 48kHz.
// TODO(fbarchard): Resample audio from higher frequency to 48000.
static const int kAlsaMaxSampleRate = 48000;

// While the "default" device may support multi-channel audio, in Alsa, only
// the device names surround40, surround41, surround50, etc, have a defined
// channel mapping according to Lennart:
//
// http://0pointer.de/blog/projects/guide-to-sound-apis.html
//
// This function makes a best guess at the specific > 2 channel device name
// based on the number of channels requested.  NULL is returned if no device
// can be found to match the channel numbers.  In this case, using
// kDefaultDevice is probably the best bet.
//
// A five channel source is assumed to be surround50 instead of surround41
// (which is also 5 channels).
//
// TODO(ajwong): The source data should have enough info to tell us if we want
// surround41 versus surround51, etc., instead of needing us to guess based on
// channel number.  Fix API to pass that data down.
static const char* GuessSpecificDeviceName(uint32 channels) {
  switch (channels) {
    case 8:
      return "surround71";

    case 7:
      return "surround70";

    case 6:
      return "surround51";

    case 5:
      return "surround50";

    case 4:
      return "surround40";

    default:
      return NULL;
  }
}

std::ostream& operator<<(std::ostream& os,
                         AlsaPcmOutputStream::InternalState state) {
  switch (state) {
    case AlsaPcmOutputStream::kInError:
      os << "kInError";
      break;
    case AlsaPcmOutputStream::kCreated:
      os << "kCreated";
      break;
    case AlsaPcmOutputStream::kIsOpened:
      os << "kIsOpened";
      break;
    case AlsaPcmOutputStream::kIsPlaying:
      os << "kIsPlaying";
      break;
    case AlsaPcmOutputStream::kIsStopped:
      os << "kIsStopped";
      break;
    case AlsaPcmOutputStream::kIsClosed:
      os << "kIsClosed";
      break;
  };
  return os;
}

const char AlsaPcmOutputStream::kDefaultDevice[] = "default";
const char AlsaPcmOutputStream::kAutoSelectDevice[] = "";
const char AlsaPcmOutputStream::kPlugPrefix[] = "plug:";

// We use 40ms as our minimum required latency. If it is needed, we may be able
// to get it down to 20ms.
const uint32 AlsaPcmOutputStream::kMinLatencyMicros = 40 * 1000;

AlsaPcmOutputStream::AlsaPcmOutputStream(const std::string& device_name,
                                         const AudioParameters& params,
                                         AlsaWrapper* wrapper,
                                         AudioManagerLinux* manager)
    : requested_device_name_(device_name),
      pcm_format_(alsa_util::BitsToFormat(params.bits_per_sample())),
      channels_(params.channels()),
      sample_rate_(params.sample_rate()),
      bytes_per_sample_(params.bits_per_sample() / 8),
      bytes_per_frame_(channels_ * params.bits_per_sample() / 8),
      should_downmix_(false),
      packet_size_(params.GetBytesPerBuffer()),
      micros_per_packet_(FramesToMicros(
          params.frames_per_buffer(), sample_rate_)),
      latency_micros_(std::max(AlsaPcmOutputStream::kMinLatencyMicros,
                               micros_per_packet_ * 2)),
      bytes_per_output_frame_(bytes_per_frame_),
      alsa_buffer_frames_(0),
      stop_stream_(false),
      wrapper_(wrapper),
      manager_(manager),
      message_loop_(MessageLoop::current()),
      playback_handle_(NULL),
      frames_per_packet_(packet_size_ / bytes_per_frame_),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      state_(kCreated),
      volume_(1.0f),
      source_callback_(NULL),
      audio_bus_(AudioBus::Create(params)) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  DCHECK_EQ(audio_bus_->frames() * bytes_per_frame_, packet_size_);

  // Sanity check input values.
  if (params.sample_rate() > kAlsaMaxSampleRate ||
      params.sample_rate() <= 0) {
    LOG(WARNING) << "Unsupported audio frequency.";
    TransitionTo(kInError);
  }

  if (AudioParameters::AUDIO_PCM_LINEAR != params.format() &&
      AudioParameters::AUDIO_PCM_LOW_LATENCY != params.format()) {
    LOG(WARNING) << "Unsupported audio format";
    TransitionTo(kInError);
  }

  if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
    LOG(WARNING) << "Unsupported bits per sample: " << params.bits_per_sample();
    TransitionTo(kInError);
  }
}

AlsaPcmOutputStream::~AlsaPcmOutputStream() {
  InternalState current_state = state();
  DCHECK(current_state == kCreated ||
         current_state == kIsClosed ||
         current_state == kInError);
  DCHECK(!playback_handle_);
}

bool AlsaPcmOutputStream::Open() {
  DCHECK(IsOnAudioThread());

  if (state() == kInError)
    return false;

  if (!CanTransitionTo(kIsOpened)) {
    NOTREACHED() << "Invalid state: " << state();
    return false;
  }

  // We do not need to check if the transition was successful because
  // CanTransitionTo() was checked above, and it is assumed that this
  // object's public API is only called on one thread so the state cannot
  // transition out from under us.
  TransitionTo(kIsOpened);

  // Try to open the device.
  if (requested_device_name_ == kAutoSelectDevice) {
    playback_handle_ = AutoSelectDevice(latency_micros_);
    if (playback_handle_)
      DVLOG(1) << "Auto-selected device: " << device_name_;
  } else {
    device_name_ = requested_device_name_;
    playback_handle_ = alsa_util::OpenPlaybackDevice(
        wrapper_, device_name_.c_str(), channels_, sample_rate_,
        pcm_format_, latency_micros_);
  }

  // Finish initializing the stream if the device was opened successfully.
  if (playback_handle_ == NULL) {
    stop_stream_ = true;
    TransitionTo(kInError);
    return false;
  } else {
    bytes_per_output_frame_ = should_downmix_ ? 2 * bytes_per_sample_ :
        bytes_per_frame_;
    uint32 output_packet_size = frames_per_packet_ * bytes_per_output_frame_;
    buffer_.reset(new media::SeekableBuffer(0, output_packet_size));

    // Get alsa buffer size.
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    int error = wrapper_->PcmGetParams(playback_handle_, &buffer_size,
                                       &period_size);
    if (error < 0) {
      LOG(ERROR) << "Failed to get playback buffer size from ALSA: "
                 << wrapper_->StrError(error);
      // Buffer size is at least twice of packet size.
      alsa_buffer_frames_ = frames_per_packet_ * 2;
    } else {
      alsa_buffer_frames_ = buffer_size;
    }
  }

  return true;
}

void AlsaPcmOutputStream::Close() {
  DCHECK(IsOnAudioThread());

  if (state() != kIsClosed)
    TransitionTo(kIsClosed);

  // Shutdown the audio device.
  if (playback_handle_) {
    if (alsa_util::CloseDevice(wrapper_, playback_handle_) < 0) {
      LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    }
    playback_handle_ = NULL;

    // Release the buffer.
    buffer_.reset();

    // Signal anything that might already be scheduled to stop.
    stop_stream_ = true;  // Not necessary in production, but unit tests
                          // uses the flag to verify that stream was closed.
  }

  weak_factory_.InvalidateWeakPtrs();

  // Signal to the manager that we're closed and can be removed.
  // Should be last call in the method as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

void AlsaPcmOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(IsOnAudioThread());

  CHECK(callback);

  if (stop_stream_)
    return;

  set_source_callback(callback);

  // Only post the task if we can enter the playing state.
  if (TransitionTo(kIsPlaying) == kIsPlaying) {
    // Before starting, the buffer might have audio from previous user of this
    // device.
    buffer_->Clear();

    // When starting again, drop all packets in the device and prepare it again
    // in case we are restarting from a pause state and need to flush old data.
    int error = wrapper_->PcmDrop(playback_handle_);
    if (error < 0 && error != -EAGAIN) {
      LOG(ERROR) << "Failure clearing playback device ("
                 << wrapper_->PcmName(playback_handle_) << "): "
                 << wrapper_->StrError(error);
      stop_stream_ = true;
    } else {
      error = wrapper_->PcmPrepare(playback_handle_);
      if (error < 0 && error != -EAGAIN) {
        LOG(ERROR) << "Failure preparing stream ("
                   << wrapper_->PcmName(playback_handle_) << "): "
                   << wrapper_->StrError(error);
        stop_stream_ = true;
      }
    }

    if (!stop_stream_)
      WriteTask();
  }
}

void AlsaPcmOutputStream::Stop() {
  DCHECK(IsOnAudioThread());

  // Reset the callback, so that it is not called anymore.
  set_source_callback(NULL);

  TransitionTo(kIsStopped);
}

void AlsaPcmOutputStream::SetVolume(double volume) {
  DCHECK(IsOnAudioThread());

  volume_ = static_cast<float>(volume);
}

void AlsaPcmOutputStream::GetVolume(double* volume) {
  DCHECK(IsOnAudioThread());

  *volume = volume_;
}

void AlsaPcmOutputStream::BufferPacket(bool* source_exhausted) {
  DCHECK(IsOnAudioThread());

  // If stopped, simulate a 0-length packet.
  if (stop_stream_) {
    buffer_->Clear();
    *source_exhausted = true;
    return;
  }

  *source_exhausted = false;

  // Request more data only when we run out of data in the buffer, because
  // WritePacket() comsumes only the current chunk of data.
  if (!buffer_->forward_bytes()) {
    // Before making a request to source for data we need to determine the
    // delay (in bytes) for the requested data to be played.

    uint32 buffer_delay = buffer_->forward_bytes() * bytes_per_frame_ /
        bytes_per_output_frame_;

    uint32 hardware_delay = GetCurrentDelay() * bytes_per_frame_;

    scoped_refptr<media::DataBuffer> packet =
        new media::DataBuffer(packet_size_);
    int frames_filled = RunDataCallback(
        audio_bus_.get(), AudioBuffersState(buffer_delay, hardware_delay));
    size_t packet_size = frames_filled * bytes_per_frame_;
    DCHECK_LE(packet_size, packet_size_);
    // Note: If this ever changes to output raw float the data must be clipped
    // and sanitized since it may come from an untrusted source such as NaCl.
    audio_bus_->ToInterleaved(
        frames_filled, bytes_per_sample_, packet->GetWritableData());

    // Reset the |last_fill_time| to avoid back to back RunDataCallback().
    last_fill_time_ = base::Time::Now();

    // TODO(dalecurtis): Channel downmixing, upmixing, should be done in mixer;
    // volume adjust should use SSE optimized vector_fmul() prior to interleave.
    if (should_downmix_) {
      if (media::FoldChannels(packet->GetWritableData(),
                              packet_size,
                              channels_,
                              bytes_per_sample_,
                              volume_)) {
        // Adjust packet size for downmix.
        packet_size = packet_size / bytes_per_frame_ * bytes_per_output_frame_;
      } else {
        LOG(ERROR) << "Folding failed";
      }
    } else {
      media::AdjustVolume(packet->GetWritableData(),
                          packet_size,
                          channels_,
                          bytes_per_sample_,
                          volume_);
    }

    if (packet_size > 0) {
      packet->SetDataSize(packet_size);
      // Add the packet to the buffer.
      buffer_->Append(packet);
    } else {
      *source_exhausted = true;
    }
  }
}

void AlsaPcmOutputStream::WritePacket() {
  DCHECK(IsOnAudioThread());

  // If the device is in error, just eat the bytes.
  if (stop_stream_) {
    buffer_->Clear();
    return;
  }

  if (state() != kIsPlaying)
    return;

  CHECK_EQ(buffer_->forward_bytes() % bytes_per_output_frame_, 0u);

  const uint8* buffer_data;
  int buffer_size;
  if (buffer_->GetCurrentChunk(&buffer_data, &buffer_size)) {
    buffer_size = buffer_size - (buffer_size % bytes_per_output_frame_);
    snd_pcm_sframes_t frames = std::min(
        static_cast<snd_pcm_sframes_t>(buffer_size / bytes_per_output_frame_),
        GetAvailableFrames());

    snd_pcm_sframes_t frames_written =
        wrapper_->PcmWritei(playback_handle_, buffer_data, frames);
    if (frames_written < 0) {
      // Attempt once to immediately recover from EINTR,
      // EPIPE (overrun/underrun), ESTRPIPE (stream suspended).  WritePacket
      // will eventually be called again, so eventual recovery will happen if
      // muliple retries are required.
      frames_written = wrapper_->PcmRecover(playback_handle_,
                                            frames_written,
                                            kPcmRecoverIsSilent);
      if (frames_written < 0) {
        if (frames_written != -EAGAIN) {
          LOG(ERROR) << "Failed to write to pcm device: "
                     << wrapper_->StrError(frames_written);
          RunErrorCallback(frames_written);
          stop_stream_ = true;
        }
      }
    } else {
      DCHECK_EQ(frames_written, frames);

      // Seek forward in the buffer after we've written some data to ALSA.
      buffer_->Seek(frames_written * bytes_per_output_frame_);
    }
  } else {
    // If nothing left to write and playback hasn't started yet, start it now.
    // This ensures that shorter sounds will still play.
    if (playback_handle_ &&
        (wrapper_->PcmState(playback_handle_) == SND_PCM_STATE_PREPARED) &&
        GetCurrentDelay() > 0) {
      wrapper_->PcmStart(playback_handle_);
    }
  }
}

void AlsaPcmOutputStream::WriteTask() {
  DCHECK(IsOnAudioThread());

  if (stop_stream_)
    return;

  if (state() == kIsStopped)
    return;

  bool source_exhausted;
  BufferPacket(&source_exhausted);
  WritePacket();

  ScheduleNextWrite(source_exhausted);
}

void AlsaPcmOutputStream::ScheduleNextWrite(bool source_exhausted) {
  DCHECK(IsOnAudioThread());

  if (stop_stream_)
    return;

  const uint32 kTargetFramesAvailable = alsa_buffer_frames_ / 2;
  uint32 available_frames = GetAvailableFrames();
  uint32 frames_in_buffer = buffer_->forward_bytes() / bytes_per_output_frame_;

  // Next write is initially scheduled for the moment when half of a packet
  // has been played out.
  uint32 next_fill_time_ms =
      FramesToMillis(frames_per_packet_ / 2, sample_rate_);

  if (frames_in_buffer && available_frames) {
    // There is data in the current buffer, consume them immediately once we
    // have enough space in the soundcard.
    if (frames_in_buffer <= available_frames)
      next_fill_time_ms = 0;
  } else {
    // Otherwise schedule the next write for the moment when the available
    // buffer of the soundcards hits the |kTargetFramesAvailable|.
    if (available_frames < kTargetFramesAvailable) {
      uint32 frames_until_empty_enough =
          kTargetFramesAvailable - available_frames;
      next_fill_time_ms =
          FramesToMillis(frames_until_empty_enough, sample_rate_);

      // Adjust for the kernel timeslice and any additional slowdown.
      // TODO(xians): Remove this adjustment if it is not required by
      // low performance machines any more.
      if (next_fill_time_ms > kSleepErrorMilliseconds)
        next_fill_time_ms -= kSleepErrorMilliseconds;
      else
        next_fill_time_ms = 0;
    } else {
      // The sound card has |kTargetFramesAvailable| or more frames available.
      // Invoke the next write immediately to avoid underrun.
      next_fill_time_ms = 0;
    }

    // Avoid back-to-back writing.
    base::TimeDelta delay = base::Time::Now() - last_fill_time_;
    if (delay.InMilliseconds() < kMinIntervalBetweenOnMoreDataCallsInMs &&
        next_fill_time_ms < kMinIntervalBetweenOnMoreDataCallsInMs)
      next_fill_time_ms = kMinIntervalBetweenOnMoreDataCallsInMs;
  }

  // Avoid busy looping if the data source is exhausted.
  if (source_exhausted)
    next_fill_time_ms = std::max(next_fill_time_ms, kNoDataSleepMilliseconds);

  // Only schedule more reads/writes if we are still in the playing state.
  if (state() == kIsPlaying) {
    message_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AlsaPcmOutputStream::WriteTask,
                   weak_factory_.GetWeakPtr()),
                   base::TimeDelta::FromMilliseconds(next_fill_time_ms));
  }
}

uint32 AlsaPcmOutputStream::FramesToMicros(uint32 frames,
                                           uint32 sample_rate) {
  return frames * base::Time::kMicrosecondsPerSecond / sample_rate;
}

uint32 AlsaPcmOutputStream::FramesToMillis(uint32 frames,
                                           uint32 sample_rate) {
  return frames * base::Time::kMillisecondsPerSecond / sample_rate;
}

std::string AlsaPcmOutputStream::FindDeviceForChannels(uint32 channels) {
  // Constants specified by the ALSA API for device hints.
  static const int kGetAllDevices = -1;
  static const char kPcmInterfaceName[] = "pcm";
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";

  const char* wanted_device = GuessSpecificDeviceName(channels);
  if (!wanted_device)
    return "";

  std::string guessed_device;
  void** hints = NULL;
  int error = wrapper_->DeviceNameHint(kGetAllDevices,
                                       kPcmInterfaceName,
                                       &hints);
  if (error == 0) {
    // NOTE: Do not early return from inside this if statement.  The
    // hints above need to be freed.
    for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
      // Only examine devices that are output capable..  Valid values are
      // "Input", "Output", and NULL which means both input and output.
      scoped_ptr_malloc<char> io(
          wrapper_->DeviceNameGetHint(*hint_iter, kIoHintName));
      if (io != NULL && strcmp(io.get(), "Input") == 0)
        continue;

      // Attempt to select the closest device for number of channels.
      scoped_ptr_malloc<char> name(
          wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName));
      if (strncmp(wanted_device, name.get(), strlen(wanted_device)) == 0) {
        guessed_device = name.get();
        break;
      }
    }

    // Destroy the hint now that we're done with it.
    wrapper_->DeviceNameFreeHint(hints);
    hints = NULL;
  } else {
    LOG(ERROR) << "Unable to get hints for devices: "
               << wrapper_->StrError(error);
  }

  return guessed_device;
}

snd_pcm_sframes_t AlsaPcmOutputStream::GetCurrentDelay() {
  snd_pcm_sframes_t delay = -1;
  // Don't query ALSA's delay if we have underrun since it'll be jammed at some
  // non-zero value and potentially even negative!
  //
  // Also, if we're in the prepared state, don't query because that seems to
  // cause an I/O error when we do query the delay.
  snd_pcm_state_t pcm_state = wrapper_->PcmState(playback_handle_);
  if (pcm_state != SND_PCM_STATE_XRUN &&
      pcm_state != SND_PCM_STATE_PREPARED) {
    int error = wrapper_->PcmDelay(playback_handle_, &delay);
    if (error < 0) {
      // Assume a delay of zero and attempt to recover the device.
      delay = -1;
      error = wrapper_->PcmRecover(playback_handle_,
                                   error,
                                   kPcmRecoverIsSilent);
      if (error < 0) {
        LOG(ERROR) << "Failed querying delay: " << wrapper_->StrError(error);
      }
    }
  }

  // snd_pcm_delay() sometimes returns crazy values. In this case return delay
  // of data we know currently is in ALSA's buffer.
  if (delay < 0 || static_cast<snd_pcm_uframes_t>(delay) > alsa_buffer_frames_)
    delay = alsa_buffer_frames_ - GetAvailableFrames();

  return delay;
}

snd_pcm_sframes_t AlsaPcmOutputStream::GetAvailableFrames() {
  DCHECK(IsOnAudioThread());

  if (stop_stream_)
    return 0;

  // Find the number of frames queued in the sound device.
  snd_pcm_sframes_t available_frames =
      wrapper_->PcmAvailUpdate(playback_handle_);
  if (available_frames < 0) {
    available_frames = wrapper_->PcmRecover(playback_handle_,
                                            available_frames,
                                            kPcmRecoverIsSilent);
  }
  if (available_frames < 0) {
    LOG(ERROR) << "Failed querying available frames. Assuming 0: "
               << wrapper_->StrError(available_frames);
    return 0;
  }
  if (static_cast<uint32>(available_frames) > alsa_buffer_frames_) {
    LOG(ERROR) << "ALSA returned " << available_frames << " of "
               << alsa_buffer_frames_ << " frames available.";
    return alsa_buffer_frames_;
  }

  return available_frames;
}

snd_pcm_t* AlsaPcmOutputStream::AutoSelectDevice(unsigned int latency) {
  // For auto-selection:
  //   1) Attempt to open a device that best matches the number of channels
  //      requested.
  //   2) If that fails, attempt the "plug:" version of it in case ALSA can
  //      remap do some software conversion to make it work.
  //   3) Fallback to kDefaultDevice.
  //   4) If that fails too, try the "plug:" version of kDefaultDevice.
  //   5) Give up.
  snd_pcm_t* handle = NULL;
  device_name_ = FindDeviceForChannels(channels_);

  // Step 1.
  if (!device_name_.empty()) {
    if ((handle = alsa_util::OpenPlaybackDevice(wrapper_, device_name_.c_str(),
                                                channels_, sample_rate_,
                                                pcm_format_,
                                                latency)) != NULL) {
      return handle;
    }

    // Step 2.
    device_name_ = kPlugPrefix + device_name_;
    if ((handle = alsa_util::OpenPlaybackDevice(wrapper_, device_name_.c_str(),
                                                channels_, sample_rate_,
                                                pcm_format_,
                                                latency)) != NULL) {
      return handle;
    }
  }

  // For the kDefaultDevice device, we can only reliably depend on 2-channel
  // output to have the correct ordering according to Lennart.  For the channel
  // formats that we know how to downmix from (3 channel to 8 channel), setup
  // downmixing.
  //
  // TODO(ajwong): We need a SupportsFolding() function.
  uint32 default_channels = channels_;
  if (default_channels > 2 && default_channels <= 8) {
    should_downmix_ = true;
    default_channels = 2;
  }

  // Step 3.
  device_name_ = kDefaultDevice;
  if ((handle = alsa_util::OpenPlaybackDevice(
      wrapper_, device_name_.c_str(), default_channels, sample_rate_,
      pcm_format_, latency)) != NULL) {
    return handle;
  }

  // Step 4.
  device_name_ = kPlugPrefix + device_name_;
  if ((handle = alsa_util::OpenPlaybackDevice(
      wrapper_, device_name_.c_str(), default_channels, sample_rate_,
      pcm_format_, latency)) != NULL) {
    return handle;
  }

  // Unable to open any device.
  device_name_.clear();
  return NULL;
}

bool AlsaPcmOutputStream::CanTransitionTo(InternalState to) {
  switch (state_) {
    case kCreated:
      return to == kIsOpened || to == kIsClosed || to == kInError;

    case kIsOpened:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsPlaying:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsStopped:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kInError:
      return to == kIsClosed || to == kInError;

    case kIsClosed:
    default:
      return false;
  }
}

AlsaPcmOutputStream::InternalState
AlsaPcmOutputStream::TransitionTo(InternalState to) {
  DCHECK(IsOnAudioThread());

  if (!CanTransitionTo(to)) {
    NOTREACHED() << "Cannot transition from: " << state_ << " to: " << to;
    state_ = kInError;
  } else {
    state_ = to;
  }
  return state_;
}

AlsaPcmOutputStream::InternalState AlsaPcmOutputStream::state() {
  return state_;
}

bool AlsaPcmOutputStream::IsOnAudioThread() const {
  return message_loop_ && message_loop_ == MessageLoop::current();
}

int AlsaPcmOutputStream::RunDataCallback(AudioBus* audio_bus,
                                         AudioBuffersState buffers_state) {
  TRACE_EVENT0("audio", "AlsaPcmOutputStream::RunDataCallback");

  if (source_callback_)
    return source_callback_->OnMoreData(audio_bus, buffers_state);

  return 0;
}

void AlsaPcmOutputStream::RunErrorCallback(int code) {
  if (source_callback_)
    source_callback_->OnError(this, code);
}

// Changes the AudioSourceCallback to proxy calls to.  Pass in NULL to
// release ownership of the currently registered callback.
void AlsaPcmOutputStream::set_source_callback(AudioSourceCallback* callback) {
  DCHECK(IsOnAudioThread());
  source_callback_ = callback;
}

}  // namespace media
