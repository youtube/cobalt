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
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "perfetto/tracing/track_event_args.h"
#include "base/strings/string_number_conversions.h"
#include "media/audio/android/aaudio_output.h"
#include "media/audio/android/aaudio_stubs.h"
#include "media/audio/android/audio_track_output_stream.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/android/starboard_audio_input_stream.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/android/media_jni_headers/AudioManagerAndroid_jni.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace content { extern base::TimeTicks g_select_keydown_time; }

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
  return Java_AudioManagerAndroid_isAudioSinkConnected(
      base::android::AttachCurrentThread());
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

  // Always add default device parameters as first element.
  DCHECK(device_names->empty());
  AddDefaultDevice(device_names);

  // Get list of available audio devices.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_device_array =
      Java_AudioManagerAndroid_getAudioInputDeviceNames(env,
                                                        GetJavaAudioManager());
  if (j_device_array.is_null()) {
    // Most probable reason for a NULL result here is that the process lacks
    // MODIFY_AUDIO_SETTINGS or RECORD_AUDIO permissions.
    return;
  }
  AudioDeviceName device;
  for (auto j_device : j_device_array.ReadElements<jobject>()) {
    ScopedJavaLocalRef<jstring> j_device_name =
        Java_AudioDeviceName_name(env, j_device);
    ConvertJavaStringToUTF8(env, j_device_name.obj(), &device.device_name);
    ScopedJavaLocalRef<jstring> j_device_id =
        Java_AudioDeviceName_id(env, j_device);
    ConvertJavaStringToUTF8(env, j_device_id.obj(), &device.unique_id);
    device_names->push_back(device);
  }

  for (auto d : *device_names) {
    DVLOG(1) << "device_name: " << d.device_name;
    DVLOG(1) << "unique_id: " << d.unique_id;
  }
}

void AudioManagerAndroid::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  // TODO(henrika): enumerate using GetAudioInputDeviceNames().
  AddDefaultDevice(device_names);
}

AudioParameters AudioManagerAndroid::GetInputStreamParameters(
    const std::string& device_id) {
  
  uint64_t id = ::content::g_select_keydown_time.since_origin().InMicroseconds();
  TRACE_EVENT("media", "RecordLatency::GetInputStreamParameters", perfetto::Flow::ProcessScoped(id));
  base::TimeDelta elapsed =
      base::TimeTicks::Now() - ::content::g_select_keydown_time;
  LOG(INFO) << "KJ: AudioManagerAndroid::GetInputStreamParameters: device_id=" << device_id 
            << " latency(msec)=" << elapsed.InMilliseconds();

  // Starboard POC: Hardcode Mono to bypass JNI/Probing overhead.
  // This is now thread-safe and can be called from any thread to avoid hops.
  constexpr ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  int sample_rate = StarboardAudioInputStream::kSampleRateHz;
  int buffer_size = 512; // Adjusted for 48kHz (approx 10ms)

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::FromLayout<channel_layout>(),
                         sample_rate, buffer_size);
  params.set_effects(AudioParameters::NO_EFFECTS);
  LOG(INFO) << "KJ: " << __func__ << "params=" << params.AsHumanReadableString();
  return params;
}

const char* AudioManagerAndroid::GetName() {
  return "Android";
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
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
  
  // KJ: Check if we have a pre-started stream for this device.
  // We use the first one available or wait for it to finish opening.
  {
    base::AutoLock lock(pre_started_streams_lock_);
    if (!pre_started_streams_.empty()) {
      auto it = pre_started_streams_.begin();
      
      // Keep a raw pointer to the entry so we can wait on it.
      PreStartedEntry* entry = it->second.get();
      
      LOG(INFO) << "KJ: Re-using PRE-STARTED hardware stream. Waiting for it to finish opening...";
      
      {
        base::AutoUnlock unlock(pre_started_streams_lock_);
        entry->open_event.Wait();
      }
      
      AudioInputStream* stream = entry->stream;
      if (stream) {
        pre_started_streams_.erase(it);
        LOG(INFO) << "KJ: Successfully re-used PRE-STARTED stream";
        return stream;
      }
    }
  }

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
  
  // KJ: Remove from pre-started map if present.
  {
    base::AutoLock lock(pre_started_streams_lock_);
    for (auto it = pre_started_streams_.begin(); it != pre_started_streams_.end(); ++it) {
      if (it->second->stream == stream) {
        pre_started_streams_.erase(it);
        break;
      }
    }
  }

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
  
  uint64_t id = ::content::g_select_keydown_time.since_origin().InMicroseconds();
  TRACE_EVENT("media", "RecordLatency::MakeLowLatencyInputStream", perfetto::Flow::ProcessScoped(id));
  base::TimeDelta elapsed =
      base::TimeTicks::Now() - ::content::g_select_keydown_time;
  LOG(INFO) << "KJ: AudioManagerAndroid::MakeLowLatencyInputStream: "
                "latency(msec)="
            << elapsed.InMilliseconds();

  DVLOG(1) << "MakeLowLatencyInputStream: " << params.effects();
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
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
  return new StarboardAudioInputStream(this, params);
}

void AudioManagerAndroid::PreStartStream(const base::UnguessableToken& session_id,
                                         const AudioParameters& params) {
  LOG(INFO) << "KJ: AudioManagerAndroid::PreStartStream session=" << session_id.ToString();
  
  // We must be on the audio thread to create the stream objects.
  if (!GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioManagerAndroid::PreStartStream,
                                  base::Unretained(this), session_id, params));
    return;
  }

  // Create the entry and park it in the map before starting the slow Open() call.
  // This allows MakeAudioInputStream to find it and WAIT for it.
  {
    base::AutoLock lock(pre_started_streams_lock_);
    pre_started_streams_[session_id] = std::make_unique<PreStartedEntry>();
  }

  // KJ: Use the base class factory method so the stream is properly registered 
  // in the AudioManagerBase::input_streams_ list. This prevents the ReleaseInputStream crash.
  AudioInputStream* stream = AudioManagerBase::MakeAudioInputStream(
      params, "default", base::DoNothing());
  
  if (!stream) {
    LOG(ERROR) << "KJ: Failed to create hardware stream for pre-start";
    base::AutoLock lock(pre_started_streams_lock_);
    pre_started_streams_.erase(session_id);
    return;
  }

  if (stream->Open() == AudioInputStream::OpenOutcome::kSuccess) {
    base::AutoLock lock(pre_started_streams_lock_);
    pre_started_streams_[session_id]->stream = stream;
    pre_started_streams_[session_id]->open_event.Signal();
    LOG(INFO) << "KJ: Hardware stream PRE-STARTED and PARKED for session=" << session_id.ToString();
  } else {
    LOG(ERROR) << "KJ: Failed to pre-start hardware stream";
    base::AutoLock lock(pre_started_streams_lock_);
    pre_started_streams_.erase(session_id);
    ReleaseInputStream(stream);
  }
}

AudioManagerAndroid::PreStartedEntry::PreStartedEntry() = default;
AudioManagerAndroid::PreStartedEntry::~PreStartedEntry() = default;

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
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  JNIEnv* env = AttachCurrentThread();
  return base::Milliseconds(
      Java_AudioManagerAndroid_getOutputLatency(env, GetJavaAudioManager()));
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
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
        reinterpret_cast<intptr_t>(this)));

    // Prepare the list of audio devices and register receivers for device
    // notifications.
    Java_AudioManagerAndroid_init(base::android::AttachCurrentThread(),
                                  j_audio_manager_);
  }
  return j_audio_manager_;
}

void AudioManagerAndroid::SetCommunicationAudioModeOn(bool on) {
  DVLOG(1) << __FUNCTION__ << ": " << on;
  Java_AudioManagerAndroid_setCommunicationAudioModeOn(
      base::android::AttachCurrentThread(), GetJavaAudioManager(), on);
}

bool AudioManagerAndroid::SetAudioDevice(const std::string& device_id) {
  
  uint64_t id = ::content::g_select_keydown_time.since_origin().InMicroseconds();
  TRACE_EVENT("media", "RecordLatency::SetAudioDevice", perfetto::Flow::ProcessScoped(id));
  base::TimeDelta elapsed =
      base::TimeTicks::Now() - ::content::g_select_keydown_time;
  LOG(INFO) << "KJ: RecordLatency::SetAudioDevice: " << device_id
            << " latency(msec)=" << elapsed.InMilliseconds();

  DVLOG(1) << __FUNCTION__ << ": " << device_id;
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Send the unique device ID to the Java audio manager and make the
  // device switch. Provide an empty string to the Java audio manager
  // if the default device is selected.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_device_id = ConvertUTF8ToJavaString(
      env, device_id == AudioDeviceDescription::kDefaultDeviceId ? std::string()
                                                                 : device_id);
  bool success = Java_AudioManagerAndroid_setDevice(env, GetJavaAudioManager(),
                                            j_device_id);
  if (success && !::content::g_select_keydown_time.is_null()) {
    LOG(INFO) << "KJ: SetAudioDevice finishes";
  }
  return success;
}

int AudioManagerAndroid::GetNativeOutputSampleRate() {
  return Java_AudioManagerAndroid_getNativeOutputSampleRate(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

bool AudioManagerAndroid::IsAudioLowLatencySupported() {
  return Java_AudioManagerAndroid_isAudioLowLatencySupported(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

int AudioManagerAndroid::GetAudioLowLatencyOutputFrameSize() {
  return Java_AudioManagerAndroid_getAudioLowLatencyOutputFrameSize(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
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
  JNIEnv* env = AttachCurrentThread();
  return Java_AudioManagerAndroid_getAudioEncodingFormatsSupported(env);
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
