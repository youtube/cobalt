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

#include "media/audio/audio_util.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"

#if defined(OS_MACOSX)
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/audio/mac/audio_low_latency_output_mac.h"
#elif defined(OS_WIN)
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/base/media_switches.h"
#endif

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

  // TODO(crogers): tune this rate for best possible WebAudio performance.
  // WebRTC works well at 48kHz and a buffer size of 480 samples will be used
  // for this case. Note that exclusive mode is experimental.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio)) {
    // This sample rate will be combined with a buffer size of 256 samples
    // (see GetAudioHardwareBufferSize()), which corresponds to an output
    // delay of ~5.33ms.
    return 48000;
  }

  // Hardware sample-rate on Windows can be configured, so we must query.
  // TODO(henrika): improve possibility to specify an audio endpoint.
  // Use the default device (same as for Wave) for now to be compatible
  // or possibly remove the ERole argument completely until it is in use.
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

  // TODO(crogers): tune this size to best possible WebAudio performance.
  // WebRTC always uses 10ms for Windows and does not call this method.
  // Note that exclusive mode is experimental.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio)) {
    return 256;
  }

  // This call must be done on a COM thread configured as MTA.
  // TODO(tommi): http://code.google.com/p/chromium/issues/detail?id=103835.
  int mixing_sample_rate =
      WASAPIAudioOutputStream::HardwareSampleRate(eConsole);

  // Use different buffer sizes depening on the sample rate . The existing
  // WASAPI implementation is tuned to provide the most stable callback
  // sequence using these combinations.
  if (mixing_sample_rate % 11025 == 0)
    // Use buffer size of ~10.15873 ms.
    return (112 * (mixing_sample_rate / 11025));

  if (mixing_sample_rate % 8000 == 0)
    // Use buffer size of 10ms.
    return (80 * (mixing_sample_rate / 8000));

  LOG(ERROR) << "Unknown sample rate " << mixing_sample_rate << " detected.";
  return (mixing_sample_rate / 100);
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

#if defined(OS_WIN)

bool IsWASAPISupported() {
  // Note: that function correctly returns that Windows Server 2003 does not
  // support WASAPI.
  return base::win::GetVersion() >= base::win::VERSION_VISTA;
}

int NumberOfWaveOutBuffers() {
  // Use 4 buffers for Vista, 3 for everyone else:
  //  - The entire Windows audio stack was rewritten for Windows Vista and wave
  //    out performance was degraded compared to XP.
  //  - The regression was fixed in Windows 7 and most configurations will work
  //    with 2, but some (e.g., some Sound Blasters) still need 3.
  //  - Some XP configurations (even multi-processor ones) also need 3.
  return (base::win::GetVersion() == base::win::VERSION_VISTA) ? 4 : 3;
}

#endif

}  // namespace media
