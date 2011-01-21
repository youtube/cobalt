// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THREAD SAFETY
//
// The AlsaPcmOutputStream object's internal state is accessed by two threads:
//
//   client thread - creates the object and calls the public APIs.
//   message loop thread - executes all the internal tasks including querying
//      the data source for more data, writing to the alsa device, and closing
//      the alsa device.  It does *not* handle opening the device though.
//
// The class is designed so that most operations that read/modify the object's
// internal state are done on the message loop thread.  The exception is data
// conatined in the |shared_data_| structure.  Data in this structure needs to
// be accessed by both threads, and should only be accessed when the
// |shared_data_.lock_| is held.
//
// All member variables that are not in |shared_data_| are created/destroyed on
// the |message_loop_|.  This allows safe access to them from any task posted to
// |message_loop_|.  The values in |shared_data_| are considered to be read-only
// signals by tasks posted to |message_loop_| (see the SEMANTICS of
// |shared_data_| section below).  Because of these two constraints, tasks can,
// and must, be coded to be safe in the face of a changing |shared_data_|.
//
//
// SEMANTICS OF |shared_data_|
//
// Though |shared_data_| is accessable by both threads, the code has been
// structured so that all mutations to |shared_data_| are only done in the
// client thread.  The message loop thread only ever reads the shared data.
//
// This reduces the need for critical sections because the public API code can
// assume that no mutations occur to the |shared_data_| between queries.
//
// On the message loop side, tasks have been coded such that they can
// operate safely regardless of when state changes happen to |shared_data_|.
// Code that is sensitive to the timing of state changes are delegated to the
// |shared_data_| object so they can executed while holding
// |shared_data_.lock_|.
//
//
// SEMANTICS OF CloseTask()
//
// The CloseTask() is responsible for cleaning up any resources that were
// acquired after a successful Open().  After a CloseTask() has executed,
// scheduling of reads should stop.  Currently scheduled tasks may run, but
// they should not attempt to access any of the internal structures beyond
// querying the |stop_stream_| flag and no-oping themselves.  This will
// guarantee that eventually no more tasks will be posted into the message
// loop, and the AlsaPcmOutputStream will be able to delete itself.
//
//
// SEMANTICS OF ERROR STATES
//
// The object has two distinct error states: |shared_data_.state_| == kInError
// and |stop_stream_|.  The |shared_data_.state_| state is only settable
// by the client thread, and thus cannot be used to signal when the ALSA device
// fails due to a hardware (or other low-level) event.  The |stop_stream_|
// variable is only accessed by the message loop thread; it is used to indicate
// that the playback_handle should no longer be used either because of a
// hardware/low-level event, or because the CloseTask() has been run.
//
// When |shared_data_.state_| == kInError, all public API functions will fail
// with an error (Start() will call the OnError() function on the callback
// immediately), or no-op themselves with the exception of Close().  Even if an
// error state has been entered, if Open() has previously returned successfully,
// Close() must be called to cleanup the ALSA devices and release resources.
//
// When |stop_stream_| is set, no more commands will be made against the
// ALSA device, and playback will effectively stop.  From the client's point of
// view, it will seem that the device has just clogged and stopped requesting
// data.

#include "media/audio/linux/alsa_output.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "media/audio/audio_util.h"
#include "media/audio/linux/alsa_util.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/audio/linux/audio_manager_linux.h"
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"

// Amount of time to wait if we've exhausted the data source.  This is to avoid
// busy looping.
static const uint32 kNoDataSleepMilliseconds = 10;

// According to the linux nanosleep manpage, nanosleep on linux can miss the
// deadline by up to 10ms because the kernel timeslice is 10ms.  Give a 2x
// buffer to compensate for the timeslice, and any additional slowdowns.
static const uint32 kSleepErrorMilliseconds = 20;

// Set to 0 during debugging if you want error messages due to underrun
// events or other recoverable errors.
#if defined(NDEBUG)
static const int kPcmRecoverIsSilent = 1;
#else
static const int kPcmRecoverIsSilent = 0;
#endif

const char AlsaPcmOutputStream::kDefaultDevice[] = "default";
const char AlsaPcmOutputStream::kAutoSelectDevice[] = "";
const char AlsaPcmOutputStream::kPlugPrefix[] = "plug:";

// Since we expect to only be able to wake up with a resolution of
// kSleepErrorMilliseconds, double that for our minimum required latency.
const uint32 AlsaPcmOutputStream::kMinLatencyMicros =
    kSleepErrorMilliseconds * 2 * 1000;

namespace {

// ALSA is currently limited to 48Khz.
// TODO(fbarchard): Resample audio from higher frequency to 48000.
const int kAlsaMaxSampleRate = 48000;

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
// surround41 versus surround51, etc., instead of needing us to guess base don
// channel number.  Fix API to pass that data down.
const char* GuessSpecificDeviceName(uint32 channels) {
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

// Reorder PCM from AAC layout to Alsa layout.
// TODO(fbarchard): Switch layout when ffmpeg is updated.
template<class Format>
static void Swizzle50Layout(Format* b, uint32 filled) {
  static const uint32 kNumSurroundChannels = 5;
  Format aac[kNumSurroundChannels];
  for (uint32 i = 0; i < filled; i += sizeof(aac), b += kNumSurroundChannels) {
    memcpy(aac, b, sizeof(aac));
    b[0] = aac[1];  // L
    b[1] = aac[2];  // R
    b[2] = aac[3];  // Ls
    b[3] = aac[4];  // Rs
    b[4] = aac[0];  // C
  }
}

template<class Format>
static void Swizzle51Layout(Format* b, uint32 filled) {
  static const uint32 kNumSurroundChannels = 6;
  Format aac[kNumSurroundChannels];
  for (uint32 i = 0; i < filled; i += sizeof(aac), b += kNumSurroundChannels) {
    memcpy(aac, b, sizeof(aac));
    b[0] = aac[1];  // L
    b[1] = aac[2];  // R
    b[2] = aac[3];  // Ls
    b[3] = aac[4];  // Rs
    b[4] = aac[0];  // C
    b[5] = aac[5];  // LFE
  }
}

}  // namespace

// Not in an anonymous namespace so that it can be a friend to
// AlsaPcmOutputStream.
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

AlsaPcmOutputStream::AlsaPcmOutputStream(const std::string& device_name,
                                         AudioParameters params,
                                         AlsaWrapper* wrapper,
                                         AudioManagerLinux* manager,
                                         MessageLoop* message_loop)
    : shared_data_(MessageLoop::current()),
      requested_device_name_(device_name),
      pcm_format_(alsa_util::BitsToFormat(params.bits_per_sample)),
      channels_(params.channels),
      sample_rate_(params.sample_rate),
      bytes_per_sample_(params.bits_per_sample / 8),
      bytes_per_frame_(channels_ * params.bits_per_sample / 8),
      should_downmix_(false),
      packet_size_(params.GetPacketSize()),
      micros_per_packet_(FramesToMicros(
          params.samples_per_packet, sample_rate_)),
      latency_micros_(std::max(AlsaPcmOutputStream::kMinLatencyMicros,
                               micros_per_packet_ * 2)),
      bytes_per_output_frame_(bytes_per_frame_),
      alsa_buffer_frames_(0),
      stop_stream_(false),
      wrapper_(wrapper),
      manager_(manager),
      playback_handle_(NULL),
      frames_per_packet_(packet_size_ / bytes_per_frame_),
      client_thread_loop_(MessageLoop::current()),
      message_loop_(message_loop) {

  // Sanity check input values.
  if ((params.sample_rate > kAlsaMaxSampleRate) || (params.sample_rate <= 0)) {
    LOG(WARNING) << "Unsupported audio frequency.";
    shared_data_.TransitionTo(kInError);
  }

  if (AudioParameters::AUDIO_PCM_LINEAR != params.format) {
    LOG(WARNING) << "Only linear PCM supported.";
    shared_data_.TransitionTo(kInError);
  }

  if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
    LOG(WARNING) << "Unsupported bits per sample: " << params.bits_per_sample;
    shared_data_.TransitionTo(kInError);
  }
}

AlsaPcmOutputStream::~AlsaPcmOutputStream() {
  InternalState state = shared_data_.state();
  DCHECK(state == kCreated || state == kIsClosed || state == kInError);

  // TODO(ajwong): Ensure that CloseTask has been called and the
  // playback handle released by DCHECKing that playback_handle_ is NULL.
  // Currently, because of Bug 18217, there is a race condition on destruction
  // where the stream is not always stopped and closed, causing this to fail.
}

bool AlsaPcmOutputStream::Open() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  if (shared_data_.state() == kInError) {
    return false;
  }

  if (!shared_data_.CanTransitionTo(kIsOpened)) {
    NOTREACHED() << "Invalid state: " << shared_data_.state();
    return false;
  }

  // We do not need to check if the transition was successful because
  // CanTransitionTo() was checked above, and it is assumed that this
  // object's public API is only called on one thread so the state cannot
  // transition out from under us.
  shared_data_.TransitionTo(kIsOpened);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AlsaPcmOutputStream::OpenTask));

  return true;
}

void AlsaPcmOutputStream::Close() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  // Sanity check that the transition occurs correctly.  It is safe to
  // continue anyways because all operations for closing are idempotent.
  if (shared_data_.TransitionTo(kIsClosed) != kIsClosed) {
    NOTREACHED() << "Unable to transition Closed.";
  }

  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AlsaPcmOutputStream::CloseTask));

  // Signal to the manager that we're closed and can be removed.  Since
  // we just posted a CloseTask to the message loop, we won't be deleted
  // immediately, but it will happen soon afterwards.
  manager()->ReleaseOutputStream(this);
}

void AlsaPcmOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  CHECK(callback);

  shared_data_.set_source_callback(callback);

  // Only post the task if we can enter the playing state.
  if (shared_data_.TransitionTo(kIsPlaying) == kIsPlaying) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AlsaPcmOutputStream::StartTask));
  }
}

void AlsaPcmOutputStream::Stop() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  // Reset the callback, so that it is not called anymore.
  shared_data_.set_source_callback(NULL);

  shared_data_.TransitionTo(kIsStopped);
}

void AlsaPcmOutputStream::SetVolume(double volume) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  shared_data_.set_volume(static_cast<float>(volume));
}

void AlsaPcmOutputStream::GetVolume(double* volume) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  *volume = shared_data_.volume();
}

void AlsaPcmOutputStream::OpenTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Try to open the device.
  if (requested_device_name_ == kAutoSelectDevice) {
    playback_handle_ = AutoSelectDevice(latency_micros_);
    if (playback_handle_)
      VLOG(1) << "Auto-selected device: " << device_name_;
  } else {
    device_name_ = requested_device_name_;
    playback_handle_ = alsa_util::OpenPlaybackDevice(wrapper_,
                                                     device_name_.c_str(),
                                                     channels_, sample_rate_,
                                                     pcm_format_,
                                                     latency_micros_);
  }

  // Finish initializing the stream if the device was opened successfully.
  if (playback_handle_ == NULL) {
    stop_stream_ = true;
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
      alsa_buffer_frames_ = frames_per_packet_;
    } else {
      alsa_buffer_frames_ = buffer_size;
    }
  }
}

void AlsaPcmOutputStream::StartTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (stop_stream_) {
    return;
  }

  // When starting again, drop all packets in the device and prepare it again
  // incase we are restarting from a pause state and need to flush old data.
  int error = wrapper_->PcmDrop(playback_handle_);
  if (error < 0 && error != -EAGAIN) {
    LOG(ERROR) << "Failure clearing playback device ("
               << wrapper_->PcmName(playback_handle_) << "): "
               << wrapper_->StrError(error);
    stop_stream_ = true;
    return;
  }

  error = wrapper_->PcmPrepare(playback_handle_);
  if (error < 0 && error != -EAGAIN) {
    LOG(ERROR) << "Failure preparing stream ("
               << wrapper_->PcmName(playback_handle_) << "): "
               << wrapper_->StrError(error);
    stop_stream_ = true;
    return;
  }

  ScheduleNextWrite(false);
}

void AlsaPcmOutputStream::CloseTask() {
  // NOTE: Keep this function idempotent to handle errors that might cause
  // multiple CloseTasks to be posted.
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Shutdown the audio device.
  if (playback_handle_ &&
      alsa_util::CloseDevice(wrapper_, playback_handle_) < 0) {
    LOG(WARNING) << "Unable to close audio device. Leaking handle.";
  }
  playback_handle_ = NULL;

  // Release the buffer.
  buffer_.reset();

  // Signal anything that might already be scheduled to stop.
  stop_stream_ = true;
}

void AlsaPcmOutputStream::BufferPacket(bool* source_exhausted) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // If stopped, simulate a 0-lengthed packet.
  if (stop_stream_) {
    buffer_->Clear();
    *source_exhausted = true;
    return;
  }

  *source_exhausted = false;

  // Request more data if we have capacity.
  if (buffer_->forward_capacity() > buffer_->forward_bytes()) {
    // Before making a request to source for data we need to determine the
    // delay (in bytes) for the requested data to be played.

    uint32 buffer_delay = buffer_->forward_bytes() * bytes_per_frame_ /
        bytes_per_output_frame_;

    uint32 hardware_delay = GetCurrentDelay() * bytes_per_frame_;

    scoped_refptr<media::DataBuffer> packet =
        new media::DataBuffer(packet_size_);
    size_t packet_size =
        shared_data_.OnMoreData(
            this, packet->GetWritableData(), packet->GetBufferSize(),
            AudioBuffersState(buffer_delay, hardware_delay));
    CHECK(packet_size <= packet->GetBufferSize()) <<
        "Data source overran buffer.";

    // This should not happen, but in case it does, drop any trailing bytes
    // that aren't large enough to make a frame.  Without this, packet writing
    // may stall because the last few bytes in the packet may never get used by
    // WritePacket.
    DCHECK(packet_size % bytes_per_frame_ == 0);
    packet_size = (packet_size / bytes_per_frame_) * bytes_per_frame_;

    if (should_downmix_) {
      if (media::FoldChannels(packet->GetWritableData(),
                              packet_size,
                              channels_,
                              bytes_per_sample_,
                              shared_data_.volume())) {
        // Adjust packet size for downmix.
        packet_size =
            packet_size / bytes_per_frame_ * bytes_per_output_frame_;
      } else {
        LOG(ERROR) << "Folding failed";
      }
    } else {
      // TODO(ajwong): Handle other channel orderings.

      // Handle channel order for 5.0 audio.
      if (channels_ == 5) {
        if (bytes_per_sample_ == 1) {
          Swizzle50Layout(packet->GetWritableData(), packet_size);
        } else if (bytes_per_sample_ == 2) {
          Swizzle50Layout(packet->GetWritableData(), packet_size);
        } else if (bytes_per_sample_ == 4) {
          Swizzle50Layout(packet->GetWritableData(), packet_size);
        }
      }

      // Handle channel order for 5.1 audio.
      if (channels_ == 6) {
        if (bytes_per_sample_ == 1) {
          Swizzle51Layout(packet->GetWritableData(), packet_size);
        } else if (bytes_per_sample_ == 2) {
          Swizzle51Layout(packet->GetWritableData(), packet_size);
        } else if (bytes_per_sample_ == 4) {
          Swizzle51Layout(packet->GetWritableData(), packet_size);
        }
      }

      media::AdjustVolume(packet->GetWritableData(),
                          packet_size,
                          channels_,
                          bytes_per_sample_,
                          shared_data_.volume());
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
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // If the device is in error, just eat the bytes.
  if (stop_stream_) {
    buffer_->Clear();
    return;
  }

  CHECK_EQ(buffer_->forward_bytes() % bytes_per_output_frame_, 0u);

  const uint8* buffer_data;
  size_t buffer_size;
  if (buffer_->GetCurrentChunk(&buffer_data, &buffer_size)) {
    buffer_size = buffer_size - (buffer_size % bytes_per_output_frame_);
    snd_pcm_sframes_t frames = buffer_size / bytes_per_output_frame_;

    DCHECK_GT(frames, 0);

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
    }

    if (frames_written < 0) {
      // TODO(ajwong): Is EAGAIN the only error we want to except from stopping
      // the pcm playback?
      if (frames_written != -EAGAIN) {
        LOG(ERROR) << "Failed to write to pcm device: "
                   << wrapper_->StrError(frames_written);
        shared_data_.OnError(this, frames_written);
        stop_stream_ = true;
      }
    } else {
      if (frames_written > frames) {
        LOG(WARNING)
            << "snd_pcm_writei() has written more frame that we asked.";
        frames_written = frames;
      }

      // Seek forward in the buffer after we've written some data to ALSA.
      buffer_->Seek(frames_written * bytes_per_output_frame_);
    }
  }
}

void AlsaPcmOutputStream::WriteTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (stop_stream_) {
    return;
  }

  bool source_exhausted;
  BufferPacket(&source_exhausted);
  WritePacket();

  ScheduleNextWrite(source_exhausted);
}

void AlsaPcmOutputStream::ScheduleNextWrite(bool source_exhausted) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (stop_stream_) {
    return;
  }

  // Next write is scheduled for the moment when half of the buffer is
  // available.
  uint32 frames_avail_wanted = alsa_buffer_frames_ / 2;
  uint32 available_frames = GetAvailableFrames();
  uint32 next_fill_time_ms = 0;

  // It's possible to have more frames available than what we want, in which
  // case we'll leave our |next_fill_time_ms| at 0ms.
  if (available_frames < frames_avail_wanted) {
    uint32 frames_until_empty_enough = frames_avail_wanted - available_frames;
    next_fill_time_ms =
        FramesToMillis(frames_until_empty_enough, sample_rate_);
  }

  // Adjust for timer resolution issues.
  if (next_fill_time_ms < kSleepErrorMilliseconds) {
    next_fill_time_ms = 0;
  } else {
    next_fill_time_ms -= kSleepErrorMilliseconds;
  }

  // Avoid busy looping if the data source is exhausted.
  if (source_exhausted) {
    next_fill_time_ms = std::max(next_fill_time_ms, kNoDataSleepMilliseconds);
  }

  // Only schedule more reads/writes if we are still in the playing state.
  if (shared_data_.state() == kIsPlaying) {
    if (next_fill_time_ms == 0) {
      message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &AlsaPcmOutputStream::WriteTask));
    } else {
      // TODO(ajwong): Measure the reliability of the delay interval.  Use
      // base/metrics/histogram.h.
      message_loop_->PostDelayedTask(
          FROM_HERE,
          NewRunnableMethod(this, &AlsaPcmOutputStream::WriteTask),
          next_fill_time_ms);
    }
  }
}

uint32 AlsaPcmOutputStream::FramesToMicros(uint32 frames, uint32 sample_rate) {
  return frames * base::Time::kMicrosecondsPerSecond / sample_rate;
}

uint32 AlsaPcmOutputStream::FramesToMillis(uint32 frames, uint32 sample_rate) {
  return frames * base::Time::kMillisecondsPerSecond / sample_rate;
}

std::string AlsaPcmOutputStream::FindDeviceForChannels(uint32 channels) {
  // Constants specified by the ALSA API for device hints.
  static const int kGetAllDevices = -1;
  static const char kPcmInterfaceName[] = "pcm";
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";

  const char* wanted_device = GuessSpecificDeviceName(channels);
  if (!wanted_device) {
    return "";
  }

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

    // Destory the hint now that we're done with it.
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

  // Don't query ALSA's delay if we have underrun since it'll be jammed at
  // some non-zero value and potentially even negative!
  if (wrapper_->PcmState(playback_handle_) != SND_PCM_STATE_XRUN) {
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

  // snd_pcm_delay() may not work in the beginning of the stream. In this case
  // return delay of data we know currently is in the ALSA's buffer.
  if (delay < 0)
    delay = alsa_buffer_frames_ - GetAvailableFrames();

  return delay;
}

snd_pcm_sframes_t AlsaPcmOutputStream::GetAvailableFrames() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (stop_stream_) {
    return 0;
  }

  // Find the number of frames queued in the sound device.
  snd_pcm_sframes_t available_frames =
      wrapper_->PcmAvailUpdate(playback_handle_);
  if (available_frames  < 0) {
    available_frames = wrapper_->PcmRecover(playback_handle_,
                                            available_frames,
                                            kPcmRecoverIsSilent);
  }
  if (available_frames < 0) {
    LOG(ERROR) << "Failed querying available frames. Assuming 0: "
               << wrapper_->StrError(available_frames);
    return 0;
  }

  return available_frames;
}

snd_pcm_t* AlsaPcmOutputStream::AutoSelectDevice(unsigned int latency) {
  // For auto-selection:
  //   1) Attempt to open a device that best matches the number of channels
  //      requested.
  //   2) If that fails, attempt the "plug:" version of it incase ALSA can
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
  if ((handle = alsa_util::OpenPlaybackDevice(wrapper_, device_name_.c_str(),
                                              default_channels, sample_rate_,
                                              pcm_format_, latency)) != NULL) {
    return handle;
  }

  // Step 4.
  device_name_ = kPlugPrefix + device_name_;
  if ((handle = alsa_util::OpenPlaybackDevice(wrapper_, device_name_.c_str(),
                                              default_channels, sample_rate_,
                                              pcm_format_, latency)) != NULL) {
    return handle;
  }

  // Unable to open any device.
  device_name_.clear();
  return NULL;
}

AudioManagerLinux* AlsaPcmOutputStream::manager() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);
  return manager_;
}

AlsaPcmOutputStream::SharedData::SharedData(
    MessageLoop* state_transition_loop)
    : state_(kCreated),
      volume_(1.0f),
      source_callback_(NULL),
      state_transition_loop_(state_transition_loop) {
}

bool AlsaPcmOutputStream::SharedData::CanTransitionTo(InternalState to) {
  base::AutoLock l(lock_);
  return CanTransitionTo_Locked(to);
}

bool AlsaPcmOutputStream::SharedData::CanTransitionTo_Locked(
    InternalState to) {
  lock_.AssertAcquired();

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
AlsaPcmOutputStream::SharedData::TransitionTo(InternalState to) {
  DCHECK_EQ(MessageLoop::current(), state_transition_loop_);

  base::AutoLock l(lock_);
  if (!CanTransitionTo_Locked(to)) {
    NOTREACHED() << "Cannot transition from: " << state_ << " to: " << to;
    state_ = kInError;
  } else {
    state_ = to;
  }
  return state_;
}

AlsaPcmOutputStream::InternalState AlsaPcmOutputStream::SharedData::state() {
  base::AutoLock l(lock_);
  return state_;
}

float AlsaPcmOutputStream::SharedData::volume() {
  base::AutoLock l(lock_);
  return volume_;
}

void AlsaPcmOutputStream::SharedData::set_volume(float v) {
  base::AutoLock l(lock_);
  volume_ = v;
}

uint32 AlsaPcmOutputStream::SharedData::OnMoreData(
    AudioOutputStream* stream, uint8* dest, uint32 max_size,
    AudioBuffersState buffers_state) {
  base::AutoLock l(lock_);
  if (source_callback_) {
    return source_callback_->OnMoreData(stream, dest, max_size, buffers_state);
  }

  return 0;
}

void AlsaPcmOutputStream::SharedData::OnError(AudioOutputStream* stream,
                                              int code) {
  base::AutoLock l(lock_);
  if (source_callback_) {
    source_callback_->OnError(stream, code);
  }
}

// Changes the AudioSourceCallback to proxy calls to.  Pass in NULL to
// release ownership of the currently registered callback.
void AlsaPcmOutputStream::SharedData::set_source_callback(
    AudioSourceCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), state_transition_loop_);
  base::AutoLock l(lock_);
  source_callback_ = callback;
}
