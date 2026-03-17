// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/audio/android/aaudio_output.h"
#include "media/audio/android/aaudio_stubs.h"
#include "media/audio/android/audio_track_output_stream.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/android/media_jni_headers/AudioManagerAndroid_jni.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

using media_audio_android::InitializeStubs;
using media_audio_android::kModuleAaudio;
using media_audio_android::StubPathMap;

static const base::FilePath::CharType kAaudioLib[] =
    FILE_PATH_LITERAL("libaaudio.so");

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 10;

const int kDefaultInputBufferSize = 1024;
const int kDefaultOutputBufferSize = 2048;

void AddDefaultDevice(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(AudioDeviceName::CreateDefault());
}

// Returns whether the currently connected device is an audio sink.
bool IsAudioSinkConnected() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  return true;
#else
  return Java_AudioManagerAndroid_isAudioSinkConnected(
      base::android::AttachCurrentThread());
#endif
}

}  // namespace

static bool InitAAudio() {
  StubPathMap paths;

  // Check if the AAudio library is available.
  paths[kModuleAaudio].push_back(kAaudioLib);
  if (!InitializeStubs(paths)) {
    VLOG(1) << "Failed on loading the AAudio library and symbols";
    return false;
  }
  return true;
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerAndroid>(std::move(audio_thread),
                                               audio_log_factory);
}

AudioManagerAndroid::AudioManagerAndroid(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      communication_mode_is_on_(false),
      output_volume_override_set_(false),
      output_volume_override_(0) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerAndroid::~AudioManagerAndroid() = default;

void AudioManagerAndroid::InitializeIfNeeded() {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(
                                    &AudioManagerAndroid::GetJavaAudioManager),
                                base::Unretained(this)));
}

void AudioManagerAndroid::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();

  // Destory java android manager here because it can only be accessed on the
  // audio thread.
  if (!j_audio_manager_.is_null()) {
    DVLOG(2) << "Destroying Java part of the audio manager";
    Java_AudioManagerAndroid_close(base::android::AttachCurrentThread(),
                                   j_audio_manager_);
    j_audio_manager_.Reset();
  }
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return true;
}

void AudioManagerAndroid::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(device_names->empty());

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  // COBALT MOD: Bypassing device enumeration for Starboard media.
  LOG(INFO) << "YO THOR COBALT MOD: Bypassing Java audio input device enumeration.";
  AddDefaultDevice(device_names);
#else
  base::AutoLock lock(device_list_lock_);
  if (input_devices_cache_.has_value() &&
      base::TimeTicks::Now() - last_input_devices_update_ < kDeviceListCacheTime) {
    *device_names = input_devices_cache_.value();
    DVLOG(2) << "GetAudioInputDeviceNames from cache";
    return;
  }

  DVLOG(2) << "GetAudioInputDeviceNames from Java";
  // Get list of available audio devices.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_device_array =
      Java_AudioManagerAndroid_getAudioInputDeviceNames(env,
                                                        GetJavaAudioManager());
  if (j_device_array.is_null()) {
    return;
  }

  input_devices_cache_ = AudioDeviceNames();
  AddDefaultDevice(&input_devices_cache_.value());

  AudioDeviceName device;
  for (auto j_device : j_device_array.ReadElements<jobject>()) {
    ScopedJavaLocalRef<jstring> j_device_name =
        Java_AudioDeviceName_name(env, j_device);
    ConvertJavaStringToUTF8(env, j_device_name.obj(), &device.device_name);
    ScopedJavaLocalRef<jstring> j_device_id =
        Java_AudioDeviceName_id(env, j_device);
    ConvertJavaStringToUTF8(env, j_device_id.obj(), &device.unique_id);
    input_devices_cache_->push_back(device);
  }
  *device_names = input_devices_cache_.value();
  last_input_devices_update_ = base::TimeTicks::Now();
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

void AudioManagerAndroid::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(device_names->empty());

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Stubbing GetAudioOutputDeviceNames";
  AddDefaultDevice(device_names);
#else
  base::AutoLock lock(device_list_lock_);
  if (output_devices_cache_.has_value() &&
      base::TimeTicks::Now() - last_output_devices_update_ < kDeviceListCacheTime) {
    *device_names = output_devices_cache_.value();
    DVLOG(2) << "GetAudioOutputDeviceNames from cache";
    return;
  }

  DVLOG(2) << "GetAudioOutputDeviceNames from Java";
  // NOTE: This currently just adds the default device. Chromium on Android
  // doesn't really have output device enumeration. If it did, we would call
  // a Java method here.
  output_devices_cache_ = AudioDeviceNames();
  AddDefaultDevice(&output_devices_cache_.value());
  *device_names = output_devices_cache_.value();
  last_output_devices_update_ = base::TimeTicks::Now();
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

AudioParameters AudioManagerAndroid::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  // COBALT MOD: Hardcode parameters to match C25 and avoid Java calls.
  constexpr ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  const int sample_rate = 48000;  // Match c25_microphone_impl.cc
  const int buffer_size = 480;  // Match c25_microphone_impl.cc kSamplesPerBuffer

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         //ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 1),
                         ChannelLayoutConfig::FromLayout<channel_layout>(),
                         sample_rate, buffer_size);
  params.set_effects(AudioParameters::NO_EFFECTS);
  DVLOG(1) << "YO THOR COBALT MOD: " << params.AsHumanReadableString();
  return params;
#else
  // Use mono as preferred number of input channels on Android to save
  // resources. Using mono also avoids a driver issue seen on Samsung
  // Galaxy S3 and S4 devices. See http://crbug.com/256851 for details.
  JNIEnv* env = AttachCurrentThread();
  constexpr ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  int buffer_size = Java_AudioManagerAndroid_getMinInputFrameSize(
      env, GetNativeOutputSampleRate(),
      ChannelLayoutToChannelCount(channel_layout));
  buffer_size = buffer_size <= 0 ? kDefaultInputBufferSize : buffer_size;
  int effects = AudioParameters::NO_EFFECTS;
  effects |= Java_AudioManagerAndroid_acousticEchoCancelerIsAvailable(env)
                 ? AudioParameters::ECHO_CANCELLER
                 : AudioParameters::NO_EFFECTS;

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::FromLayout<channel_layout>(),
                         GetNativeOutputSampleRate(), buffer_size);
  params.set_effects(effects);
  DVLOG(1) << params.AsHumanReadableString();
  return params;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

const char* AudioManagerAndroid::GetName() {
  return "Android";
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  LOG(INFO) << "YO THOR: Output requested! Params: " << params.AsHumanReadableString();
  AudioOutputStream* stream = AudioManagerBase::MakeAudioOutputStream(
      params, device_id, AudioManager::LogCallback());
  if (stream)
    streams_.insert(static_cast<MuteableAudioOutputStream*>(stream));
  return stream;
}

AudioInputStream* AudioManagerAndroid::MakeAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool has_no_input_streams = HasNoAudioInputStreams();
  AudioInputStream* stream = AudioManagerBase::MakeAudioInputStream(
      params, device_id, AudioManager::LogCallback());

  // By default, the audio manager for Android creates streams intended for
  // real-time VoIP sessions and therefore sets the audio mode to
  // MODE_IN_COMMUNICATION. However, the user might have asked for a special
  // mode where all audio input processing is disabled, and if that is the case
  // we avoid changing the mode.
  if (stream && has_no_input_streams &&
      params.effects() != AudioParameters::NO_EFFECTS) {
    communication_mode_is_on_ = true;
    SetCommunicationAudioModeOn(true);
  }
  return stream;
}

void AudioManagerAndroid::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  streams_.erase(static_cast<MuteableAudioOutputStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerAndroid::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioManagerBase::ReleaseInputStream(stream);

  // Restore the audio mode which was used before the first communication-
  // mode stream was created.
  if (HasNoAudioInputStreams() && communication_mode_is_on_) {
    communication_mode_is_on_ = false;
    SetCommunicationAudioModeOn(false);
  }
}

AudioOutputStream* AudioManagerAndroid::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (UseAAudio())
    return new AAudioOutputStream(this, params, AAUDIO_USAGE_MEDIA);

  return new OpenSLESOutputStream(this, params, SL_ANDROID_STREAM_MEDIA);
}

AudioOutputStream* AudioManagerAndroid::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (UseAAudio()) {
    const aaudio_usage_t usage = communication_mode_is_on_
                                     ? AAUDIO_USAGE_VOICE_COMMUNICATION
                                     : AAUDIO_USAGE_MEDIA;
    return new AAudioOutputStream(this, params, usage);
  }

  // Set stream type which matches the current system-wide audio mode used by
  // the Android audio manager.
  const SLint32 stream_type = communication_mode_is_on_
                                  ? SL_ANDROID_STREAM_VOICE
                                  : SL_ANDROID_STREAM_MEDIA;
  return new OpenSLESOutputStream(this, params, stream_type);
}

AudioOutputStream* AudioManagerAndroid::MakeBitstreamOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(params.IsBitstreamFormat());
  return new AudioTrackOutputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  // TODO(henrika): add support for device selection if/when any client
  // needs it.
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESInputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DVLOG(1) << "MakeLowLatencyInputStream: " << params.effects();
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Forcing OpenSLESInputStream for low latency input.";
  // COBALT NUCLEAR OPTION: Mute all playback to release the hardware clock lock
  for (auto* stream : streams_) {
    stream->SetVolume(0.0);
  }

  return new OpenSLESInputStream(this, params);
#else
  DLOG_IF(ERROR, device_id.empty()) << "Invalid device ID!";

  // Use the device ID to select the correct input device.
  // Note that the input device is always associated with a certain output
  // device, i.e., this selection does also switch the output device.
  // All input and output streams will be affected by the device selection.
  if (!SetAudioDevice(device_id)) {
    LOG(ERROR) << "Unable to select audio device!";
    return NULL;
  }

  // Create a new audio input stream and enable or disable all audio effects
  // given |params.effects()|.
  return new OpenSLESInputStream(this, params);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

// static
bool AudioManagerAndroid::SupportsPerformanceModeForOutput() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_NOUGAT_MR1;
}

void AudioManagerAndroid::SetMute(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jboolean muted) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerAndroid::DoSetMuteOnAudioThread,
                                base::Unretained(this), muted));
}

void AudioManagerAndroid::SetOutputVolumeOverride(double volume) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerAndroid::DoSetVolumeOnAudioThread,
                                base::Unretained(this), volume));
}

bool AudioManagerAndroid::HasOutputVolumeOverride(double* out_volume) const {
  if (output_volume_override_set_) {
    *out_volume = output_volume_override_;
  }
  return output_volume_override_set_;
}

base::TimeDelta AudioManagerAndroid::GetOutputLatency() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  return base::Milliseconds(50); // Cobalt default
#else
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  JNIEnv* env = AttachCurrentThread();
  return base::Milliseconds(
      Java_AudioManagerAndroid_getOutputLatency(env, GetJavaAudioManager()));
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Stubbing GetPreferredOutputStreamParameters";
  // COBALT MOD: Return default parameters to avoid Java calls.
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 1),
                         //ChannelLayoutConfig::Mono(),
                         48000, 480);
#else
  DVLOG(1) << __FUNCTION__;
  // TODO(tommi): Support |output_device_id|.
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = GetNativeOutputSampleRate();
  int buffer_size = GetOptimalOutputFrameSize(sample_rate, 2);
  if (input_params.IsValid()) {
    // Use the client's input parameters if they are valid.
    sample_rate = input_params.sample_rate();

    // AudioManager APIs for GetOptimalOutputFrameSize() don't support channel
    // layouts greater than stereo unless low latency audio is supported.
    if (input_params.channels() <= 2 || IsAudioLowLatencySupported())
      channel_layout_config = input_params.channel_layout_config();

    // For high latency playback on supported platforms, pass through the
    // requested buffer size; this provides significant power savings (~25%) and
    // reduces the potential for glitches under load.
    if (SupportsPerformanceModeForOutput() &&
        input_params.latency_tag() == AudioLatency::LATENCY_PLAYBACK) {
      buffer_size = input_params.frames_per_buffer();
    } else {
      buffer_size = GetOptimalOutputFrameSize(sample_rate,
                                              channel_layout_config.channels());
    }
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  // Check if device supports additional audio encodings.
  if (IsAudioSinkConnected()) {
    return GetAudioFormatsSupportedBySinkDevice(
        output_device_id, channel_layout_config, sample_rate, buffer_size);
  }

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

bool AudioManagerAndroid::HasNoAudioInputStreams() {
  return input_stream_count() == 0;
}

const JavaRef<jobject>& AudioManagerAndroid::GetJavaAudioManager() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (j_audio_manager_.is_null()) {
    // Create the Android audio manager on the audio thread.
    DVLOG(2) << "Creating Java part of the audio manager";
    j_audio_manager_.Reset(Java_AudioManagerAndroid_createAudioManagerAndroid(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(this),
        BUILDFLAG(USE_STARBOARD_MEDIA)));

    // Prepare the list of audio devices and register receivers for device
    // notifications.
    Java_AudioManagerAndroid_init(base::android::AttachCurrentThread(),
                                  j_audio_manager_);
  }
  return j_audio_manager_;
}

void AudioManagerAndroid::SetCommunicationAudioModeOn(bool on) {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Stubbing SetCommunicationAudioModeOn, on=" << on;
  // COBALT MOD: Do nothing to avoid Java calls to AudioManager.setMode.
#else
  DVLOG(1) << __FUNCTION__ << ": " << on;
  Java_AudioManagerAndroid_setCommunicationAudioModeOn(
      base::android::AttachCurrentThread(), GetJavaAudioManager(), on);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

bool AudioManagerAndroid::SetAudioDevice(const std::string& device_id) {
  DVLOG(1) << __FUNCTION__ << ": " << device_id;
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Send the unique device ID to the Java audio manager and make the
  // device switch. Provide an empty string to the Java audio manager
  // if the default device is selected.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_device_id = ConvertUTF8ToJavaString(
      env, device_id == AudioDeviceDescription::kDefaultDeviceId ? std::string()
                                                                 : device_id);
  return Java_AudioManagerAndroid_setDevice(env, GetJavaAudioManager(),
                                            j_device_id);
}

int AudioManagerAndroid::GetNativeOutputSampleRate() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Stubbing GetNativeOutputSampleRate";
  // COBALT MOD: Return default sample rate to avoid Java calls.
  return 48000;
#else
  return Java_AudioManagerAndroid_getNativeOutputSampleRate(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

bool AudioManagerAndroid::IsAudioLowLatencySupported() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Stubbing IsAudioLowLatencySupported";
  // COBALT MOD: Assume low latency is supported to avoid Java calls.
  return true;
#else
  return Java_AudioManagerAndroid_isAudioLowLatencySupported(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

int AudioManagerAndroid::GetAudioLowLatencyOutputFrameSize() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  LOG(INFO) << "YO THOR COBALT MOD: Stubbing GetAudioLowLatencyOutputFrameSize";
  // COBALT MOD: Return default frame size to avoid Java calls.
  return 128;
#else
  return Java_AudioManagerAndroid_getAudioLowLatencyOutputFrameSize(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

int AudioManagerAndroid::GetOptimalOutputFrameSize(int sample_rate,
                                                   int channels) {
  if (IsAudioLowLatencySupported())
    return GetAudioLowLatencyOutputFrameSize();

  return std::max(kDefaultOutputBufferSize,
                  Java_AudioManagerAndroid_getMinOutputFrameSize(
                      base::android::AttachCurrentThread(),
                      sample_rate, channels));
}

// Returns a bit mask of AudioParameters::Format enum values sink device
// supports.
int AudioManagerAndroid::GetSinkAudioEncodingFormats() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  // Hardcode to PCM or return the specific bitmask YouTube needs.
  return 1;
#else
  JNIEnv* env = AttachCurrentThread();
  return Java_AudioManagerAndroid_getAudioEncodingFormatsSupported(env);
#endif
}

// Returns encoding bitstream formats supported by Sink device. Returns
// AudioParameters structure.
AudioParameters AudioManagerAndroid::GetAudioFormatsSupportedBySinkDevice(
    const std::string& output_device_id,
    const ChannelLayoutConfig& channel_layout_config,
    int sample_rate,
    int buffer_size) {
  int formats = GetSinkAudioEncodingFormats();
  DVLOG(1) << __func__ << ": IsAudioSinkConnected()==true, output_device_id="
           << output_device_id << ", Supported Encodings=" << formats;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
      sample_rate, buffer_size,
      AudioParameters::HardwareCapabilities(formats,
                                            /*require_encapsulation=*/false));
}

void AudioManagerAndroid::DoSetMuteOnAudioThread(bool muted) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  for (OutputStreams::iterator it = streams_.begin();
       it != streams_.end(); ++it) {
    (*it)->SetMute(muted);
  }
}

void AudioManagerAndroid::DoSetVolumeOnAudioThread(double volume) {
  output_volume_override_set_ = true;
  output_volume_override_ = volume;

  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  for (OutputStreams::iterator it = streams_.begin(); it != streams_.end();
       ++it) {
    (*it)->SetVolume(volume);
  }
}

bool AudioManagerAndroid::UseAAudio() {
  if (!base::FeatureList::IsEnabled(features::kUseAAudioDriver))
    return false;

  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q) {
    // We need APIs that weren't added until API Level 28. Also, AAudio crashes
    // on Android P, so only consider Q and above.
    return false;
  }

  if (!is_aaudio_available_.has_value())
    is_aaudio_available_ = InitAAudio();

  return is_aaudio_available_.value();
}

}  // namespace media
