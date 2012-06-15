// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Software adjust volume of samples, allows each audio stream its own
// volume without impacting master volume for chrome and other applications.

// Implemented as templates to allow 8, 16 and 32 bit implementations.
// 8 bit is unsigned and biased by 128.

// TODO(vrk): This file has been running pretty wild and free, and it's likely
// that a lot of the functions can be simplified and made more elegant. Revisit
// after other audio cleanup is done. (crbug.com/120319)

#include <algorithm>
#include <limits>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/shared_memory.h"
#include "base/time.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/audio/audio_manager_base.h"
#endif
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#if defined(OS_MACOSX)
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/audio/mac/audio_low_latency_output_mac.h"
#endif
#if defined(OS_WIN)
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#endif

using base::subtle::Atomic32;

const uint32 kUnknownDataSize = static_cast<uint32>(-1);

namespace media {

// TODO(fbarchard): Convert to intrinsics for better efficiency.
template<class Fixed>
static int ScaleChannel(int channel, int volume) {
  return static_cast<int>((static_cast<Fixed>(channel) * volume) >> 16);
}

template<class Format, class Fixed, int bias>
static void AdjustVolume(Format* buf_out,
                         int sample_count,
                         int fixed_volume) {
  for (int i = 0; i < sample_count; ++i) {
    buf_out[i] = static_cast<Format>(ScaleChannel<Fixed>(buf_out[i] - bias,
                                                         fixed_volume) + bias);
  }
}

static const int kChannel_L = 0;
static const int kChannel_R = 1;
static const int kChannel_C = 2;

template<class Fixed, int min_value, int max_value>
static int AddSaturated(int val, int adder) {
  Fixed sum = static_cast<Fixed>(val) + static_cast<Fixed>(adder);
  if (sum > max_value)
    return max_value;
  if (sum < min_value)
    return min_value;
  return static_cast<int>(sum);
}

// FoldChannels() downmixes multichannel (ie 5.1 Surround Sound) to Stereo.
// Left and Right channels are preserved asis, and Center channel is
// distributed equally to both sides.  To be perceptually 1/2 volume on
// both channels, 1/sqrt(2) is used instead of 1/2.
// Fixed point math is used for efficiency.  16 bits of fraction and 8,16 or 32
// bits of integer are used.
// 8 bit samples are unsigned and 128 represents 0, so a bias is removed before
// doing calculations, then readded for the final output.
template<class Format, class Fixed, int min_value, int max_value, int bias>
static void FoldChannels(Format* buf_out,
                         int sample_count,
                         const float volume,
                         int channels) {
  Format* buf_in = buf_out;
  const int center_volume = static_cast<int>(volume * 0.707f * 65536);
  const int fixed_volume = static_cast<int>(volume * 65536);

  for (int i = 0; i < sample_count; ++i) {
    int center = static_cast<int>(buf_in[kChannel_C] - bias);
    int left = static_cast<int>(buf_in[kChannel_L] - bias);
    int right = static_cast<int>(buf_in[kChannel_R] - bias);

    center = ScaleChannel<Fixed>(center, center_volume);
    left = ScaleChannel<Fixed>(left, fixed_volume);
    right = ScaleChannel<Fixed>(right, fixed_volume);

    buf_out[0] = static_cast<Format>(
        AddSaturated<Fixed, min_value, max_value>(left, center) + bias);
    buf_out[1] = static_cast<Format>(
        AddSaturated<Fixed, min_value, max_value>(right, center) + bias);

    buf_out += 2;
    buf_in += channels;
  }
}

// AdjustVolume() does an in place audio sample change.
bool AdjustVolume(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  if (volume < 0.0f || volume > 1.0f)
    return false;
  if (volume == 1.0f) {
    return true;
  } else if (volume == 0.0f) {
    memset(buf, 0, buflen);
    return true;
  }
  if (channels > 0 && channels <= 8 && bytes_per_sample > 0) {
    int sample_count = buflen / bytes_per_sample;
    const int fixed_volume = static_cast<int>(volume * 65536);
    if (bytes_per_sample == 1) {
      AdjustVolume<uint8, int32, 128>(reinterpret_cast<uint8*>(buf),
                                      sample_count,
                                      fixed_volume);
      return true;
    } else if (bytes_per_sample == 2) {
      AdjustVolume<int16, int32, 0>(reinterpret_cast<int16*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    } else if (bytes_per_sample == 4) {
      AdjustVolume<int32, int64, 0>(reinterpret_cast<int32*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    }
  }
  return false;
}

bool FoldChannels(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  if (volume < 0.0f || volume > 1.0f)
    return false;
  if (channels > 2 && channels <= 8 && bytes_per_sample > 0) {
    int sample_count = buflen / (channels * bytes_per_sample);
    if (bytes_per_sample == 1) {
      FoldChannels<uint8, int32, -128, 127, 128>(
          reinterpret_cast<uint8*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    } else if (bytes_per_sample == 2) {
      FoldChannels<int16, int32, -32768, 32767, 0>(
          reinterpret_cast<int16*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    } else if (bytes_per_sample == 4) {
      FoldChannels<int32, int64, 0x80000000, 0x7fffffff, 0>(
          reinterpret_cast<int32*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    }
  }
  return false;
}

bool DeinterleaveAudioChannel(void* source,
                              float* destination,
                              int channels,
                              int channel_index,
                              int bytes_per_sample,
                              size_t number_of_frames) {
  switch (bytes_per_sample) {
    case 1:
    {
      uint8* source8 = reinterpret_cast<uint8*>(source) + channel_index;
      const float kScale = 1.0f / 128.0f;
      for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * (static_cast<int>(*source8) - 128);
        source8 += channels;
      }
      return true;
    }

    case 2:
    {
      int16* source16 = reinterpret_cast<int16*>(source) + channel_index;
      const float kScale = 1.0f / 32768.0f;
      for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * *source16;
        source16 += channels;
      }
      return true;
    }

    case 4:
    {
      int32* source32 = reinterpret_cast<int32*>(source) + channel_index;
      const float kScale = 1.0f / 2147483648.0f;
      for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * *source32;
        source32 += channels;
      }
      return true;
    }

    default:
     break;
  }
  return false;
}

// |Format| is the destination type, |Fixed| is a type larger than |Format|
// such that operations can be made without overflowing.
template<class Format, class Fixed>
static void InterleaveFloatToInt(const std::vector<float*>& source,
                                 void* dst_bytes, size_t number_of_frames) {
  Format* destination = reinterpret_cast<Format*>(dst_bytes);
  Fixed max_value = std::numeric_limits<Format>::max();
  Fixed min_value = std::numeric_limits<Format>::min();

  Format bias = 0;
  if (!std::numeric_limits<Format>::is_signed) {
    bias = max_value / 2;
    max_value = bias;
    min_value = -(bias - 1);
  }

  int channels = source.size();
  for (int i = 0; i < channels; ++i) {
    float* channel_data = source[i];
    for (size_t j = 0; j < number_of_frames; ++j) {
      Fixed sample = max_value * channel_data[j];
      if (sample > max_value)
        sample = max_value;
      else if (sample < min_value)
        sample = min_value;

      destination[j * channels + i] = static_cast<Format>(sample) + bias;
    }
  }
}

void InterleaveFloatToInt(const std::vector<float*>& source, void* dst,
                          size_t number_of_frames, int bytes_per_sample) {
  switch (bytes_per_sample) {
    case 1:
      InterleaveFloatToInt<uint8, int32>(source, dst, number_of_frames);
      break;
    case 2:
      InterleaveFloatToInt<int16, int32>(source, dst, number_of_frames);
      break;
    case 4:
      InterleaveFloatToInt<int32, int64>(source, dst, number_of_frames);
      break;
    default:
      break;
  }
}

// TODO(enal): use template specialization and size-specific intrinsics.
//             Call is on the time-critical path, and by using SSE/AVX
//             instructions we can speed things up by ~4-8x, more for the case
//             when we have to adjust volume as well.
template<class Format, class Fixed, int min_value, int max_value, int bias>
static void MixStreams(Format* dst, Format* src, int count, float volume) {
  if (volume == 0.0f)
    return;
  if (volume == 1.0f) {
    // Most common case -- no need to adjust volume.
    for (int i = 0; i < count; ++i) {
      Fixed value = AddSaturated<Fixed, min_value, max_value>(dst[i] - bias,
                                                              src[i] - bias);
      dst[i] = static_cast<Format>(value + bias);
    }
  } else {
    // General case -- have to adjust volume before mixing.
    const int fixed_volume = static_cast<int>(volume * 65536);
    for (int i = 0; i < count; ++i) {
      Fixed adjusted_src = ScaleChannel<Fixed>(src[i] - bias, fixed_volume);
      Fixed value = AddSaturated<Fixed, min_value, max_value>(dst[i] - bias,
                                                              adjusted_src);
      dst[i] = static_cast<Format>(value + bias);
    }
  }
}

void MixStreams(void* dst,
                void* src,
                size_t buflen,
                int bytes_per_sample,
                float volume) {
  DCHECK(dst);
  DCHECK(src);
  DCHECK_GE(volume, 0.0f);
  DCHECK_LE(volume, 1.0f);
  switch (bytes_per_sample) {
    case 1:
      MixStreams<uint8, int32, -128, 127, 128>(static_cast<uint8*>(dst),
                                               static_cast<uint8*>(src),
                                               buflen,
                                               volume);
      break;
    case 2:
      DCHECK_EQ(0u, buflen % 2);
      MixStreams<int16, int32, -32768, 32767, 0>(static_cast<int16*>(dst),
                                                 static_cast<int16*>(src),
                                                 buflen / 2,
                                                 volume);
      break;
    case 4:
      DCHECK_EQ(0u, buflen % 4);
      MixStreams<int32, int64, 0x80000000, 0x7fffffff, 0>(
          static_cast<int32*>(dst),
          static_cast<int32*>(src),
          buflen / 4,
          volume);
      break;
    default:
      NOTREACHED() << "Illegal bytes per sample";
      break;
  }
}

int GetAudioHardwareSampleRate() {
#if defined(OS_MACOSX)
  // Hardware sample-rate on the Mac can be configured, so we must query.
  return AUAudioOutputStream::HardwareSampleRate();
#elif defined(OS_WIN)
  if (!IsWASAPISupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower
    // and use 48kHz as default input sample rate.
    return 48000;
  }

  // Hardware sample-rate on Windows can be configured, so we must query.
  // TODO(henrika): improve possibility to specify audio endpoint.
  // Use the default device (same as for Wave) for now to be compatible.
  return WASAPIAudioOutputStream::HardwareSampleRate(eConsole);
#elif defined(OS_ANDROID)
  return 16000;
#else
    // Hardware for Linux is nearly always 48KHz.
    // TODO(crogers) : return correct value in rare non-48KHz cases.
    return 48000;
#endif
}

int GetAudioInputHardwareSampleRate(const std::string& device_id) {
  // TODO(henrika): add support for device selection on all platforms.
  // Only exists on Windows today.
#if defined(OS_MACOSX)
  return AUAudioInputStream::HardwareSampleRate();
#elif defined(OS_WIN)
  if (!IsWASAPISupported()) {
    return 48000;
  }
  return WASAPIAudioInputStream::HardwareSampleRate(device_id);
#elif defined(OS_ANDROID)
  return 16000;
#else
  return 48000;
#endif
}

size_t GetAudioHardwareBufferSize() {
  // The sizes here were determined by experimentation and are roughly
  // the lowest value (for low latency) that still allowed glitch-free
  // audio under high loads.
  //
  // For Mac OS X and Windows the chromium audio backend uses a low-latency
  // Core Audio API, so a low buffer size is possible. For Linux, further
  // tuning may be needed.
#if defined(OS_MACOSX)
  return 128;
#elif defined(OS_WIN)
  if (!IsWASAPISupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower
    // and assume 48kHz as default sample rate.
    return 2048;
  }
  // This call must be done on a COM thread configured as MTA.
  // TODO(tommi): http://code.google.com/p/chromium/issues/detail?id=103835.
  int mixing_sample_rate =
      WASAPIAudioOutputStream::HardwareSampleRate(eConsole);
  if (mixing_sample_rate == 48000)
    return 480;
  else if (mixing_sample_rate == 44100)
    return 448;
  else
    return 960;
#else
  return 2048;
#endif
}

ChannelLayout GetAudioInputHardwareChannelLayout(const std::string& device_id) {
  // TODO(henrika): add support for device selection on all platforms.
  // Only exists on Windows today.
#if defined(OS_MACOSX)
  return CHANNEL_LAYOUT_MONO;
#elif defined(OS_WIN)
  if (!IsWASAPISupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower and
    // use stereo by default.
    return CHANNEL_LAYOUT_STEREO;
  }
  return WASAPIAudioInputStream::HardwareChannelCount(device_id) == 1 ?
      CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;
#else
  return CHANNEL_LAYOUT_STEREO;
#endif
}

// Computes a buffer size based on the given |sample_rate|. Must be used in
// conjunction with AUDIO_PCM_LINEAR.
size_t GetHighLatencyOutputBufferSize(int sample_rate) {
  // TODO(vrk/crogers): The buffer sizes that this function computes is probably
  // overly conservative. However, reducing the buffer size to 2048-8192 bytes
  // caused crbug.com/108396. This computation should be revisited while making
  // sure crbug.com/108396 doesn't happen again.

  // The minimum number of samples in a hardware packet.
  // This value is selected so that we can handle down to 5khz sample rate.
  static const size_t kMinSamplesPerHardwarePacket = 1024;

  // The maximum number of samples in a hardware packet.
  // This value is selected so that we can handle up to 192khz sample rate.
  static const size_t kMaxSamplesPerHardwarePacket = 64 * 1024;

  // This constant governs the hardware audio buffer size, this value should be
  // chosen carefully.
  // This value is selected so that we have 8192 samples for 48khz streams.
  static const size_t kMillisecondsPerHardwarePacket = 170;

  // Select the number of samples that can provide at least
  // |kMillisecondsPerHardwarePacket| worth of audio data.
  size_t samples = kMinSamplesPerHardwarePacket;
  while (samples <= kMaxSamplesPerHardwarePacket &&
         samples * base::Time::kMillisecondsPerSecond <
         sample_rate * kMillisecondsPerHardwarePacket) {
    samples *= 2;
  }
  return samples;
}

// When transferring data in the shared memory, first word is size of data
// in bytes. Actual data starts immediately after it.

uint32 TotalSharedMemorySizeInBytes(uint32 packet_size) {
  // Need to reserve extra 4 bytes for size of data.
  return packet_size + sizeof(Atomic32);
}

uint32 PacketSizeSizeInBytes(uint32 shared_memory_created_size) {
  return shared_memory_created_size - sizeof(Atomic32);
}

uint32 GetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                                uint32 shared_memory_size) {
  char* ptr = static_cast<char*>(shared_memory->memory()) + shared_memory_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Actual data size stored at the end of the buffer.
  uint32 actual_data_size =
      base::subtle::Acquire_Load(reinterpret_cast<volatile Atomic32*>(ptr));
  return std::min(actual_data_size, shared_memory_size);
}

void SetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                              uint32 shared_memory_size,
                              uint32 actual_data_size) {
  char* ptr = static_cast<char*>(shared_memory->memory()) + shared_memory_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Set actual data size at the end of the buffer.
  base::subtle::Release_Store(reinterpret_cast<volatile Atomic32*>(ptr),
                              actual_data_size);
}

void SetUnknownDataSize(base::SharedMemory* shared_memory,
                        uint32 shared_memory_size) {
  SetActualDataSizeInBytes(shared_memory, shared_memory_size, kUnknownDataSize);
}

bool IsUnknownDataSize(base::SharedMemory* shared_memory,
                       uint32 shared_memory_size) {
  char* ptr = static_cast<char*>(shared_memory->memory()) + shared_memory_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Actual data size stored at the end of the buffer.
  uint32 actual_data_size =
      base::subtle::Acquire_Load(reinterpret_cast<volatile Atomic32*>(ptr));
  return actual_data_size == kUnknownDataSize;
}

#if defined(OS_WIN)

bool IsWASAPISupported() {
  // Note: that function correctly returns that Windows Server 2003 does not
  // support WASAPI.
  return base::win::GetVersion() >= base::win::VERSION_VISTA;
}

#endif

}  // namespace media
