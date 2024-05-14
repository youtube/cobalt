
/*
Copyright 2018 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef RESONANCE_AUDIO_API_RESONANCE_AUDIO_API2_H_
#define RESONANCE_AUDIO_API_RESONANCE_AUDIO_API2_H_

#ifdef __linux__
#define EXPORT_API
#elif _WIN32
// EXPORT_API can be used to define the dllimport storage-class attribute.
#ifdef DLL_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#endif
//#include "api/resonance_audio_api.h"
#include <stddef.h>  // size_t declaration.
#include <stdint.h>  // int16_t declaration.

typedef int SourceId;

typedef int16_t int16;

// Rendering modes define CPU load / rendering quality balances.
// Note that this struct is C-compatible by design to be used across external
// C/C++ and C# implementations.
typedef enum _RenderingMode2 {
  // Stereo panning, i.e., this disables HRTF-based rendering.
  kStereoPanning = 0,
  // HRTF-based rendering using First Order Ambisonics, over a virtual array of
  // 8 loudspeakers arranged in a cube configuration around the listener's head.
  kBinauralLowQuality,
  // HRTF-based rendering using Second Order Ambisonics, over a virtual array of
  // 12 loudspeakers arranged in a dodecahedral configuration (using faces of
  // the dodecahedron).
  kBinauralMediumQuality,
  // HRTF-based rendering using Third Order Ambisonics, over a virtual array of
  // 26 loudspeakers arranged in a Lebedev grid: https://goo.gl/DX1wh3.
  kBinauralHighQuality,
  // Room effects only rendering. This disables HRTF-based rendering and direct
  // (dry) output of a sound object. Note that this rendering mode should *not*
  // be used for general-purpose sound object spatialization, as it will only
  // render the corresponding room effects of given sound objects without the
  // direct spatialization.
  kRoomEffectsOnly,
} RenderingMode2;

// Distance rolloff models used for distance attenuation.
// Note that this enum is C-compatible by design to be used across external
// C/C++ and C# implementations.
typedef enum _DistanceRolloffModel2 {
  // Logarithmic distance rolloff model.
  kLogarithmic = 0,
  // Linear distance rolloff model.
  kLinear,
  // Distance attenuation value will be explicitly set by the user.
  kNone,
} DistanceRolloffModel2;

// Early reflection properties of an acoustic environment.
// Note that this struct is C-compatible by design to be used across external
// C/C++ and C# implementations.
typedef struct _ReflectionProperties2 {
  // Default constructor initializing all data members to 0.
  // ReflectionProperties()
  //    : room_position{0.0f, 0.0f, 0.0f},
  //      room_rotation{0.0f, 0.0f, 0.0f, 1.0f},
  //      room_dimensions{0.0f, 0.0f, 0.0f},
  //      cutoff_frequency(0.0f),
  //      coefficients{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
  //      gain(0.0f) {}

  // Center position of the shoebox room in world space.
  float room_position[3];

  // Rotation (quaternion) of the shoebox room in world space.
  float room_rotation[4];

  // Size of the shoebox shoebox room in world space.
  float room_dimensions[3];

  // Frequency threshold for low pass filtering (-3dB cuttoff).
  float cutoff_frequency;

  // Reflection coefficients that are stored in world space as follows:
  //  [0]  (-)ive x-axis wall (left)
  //  [1]  (+)ive x-axis wall (right)
  //  [2]  (-)ive y-axis wall (bottom)
  //  [3]  (+)ive y-axis wall (top)
  //  [4]  (-)ive z-axis wall (front)
  //  [5]  (+)ive z-axis wall (back)
  float coefficients[6];

  // Uniform reflections gain which is applied to all reflections.
  float gain;
} ReflectionProperties2;

// Late reverberation properties of an acoustic environment.
// Note that this struct is C-compatible by design to be used across external
// C/C++ and C# implementations.
typedef struct _ReverbProperties2 {
  // Default constructor initializing all data members to 0.
  // ReverbProperties()
  //    : rt60_values{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
  //      gain(0.0f) {}

  // RT60's of the reverberation tail at different octave band centre
  // frequencies in seconds.
  float rt60_values[9];

  // Reverb gain.
  float gain;
} ReverbProperties2;

// Factory method to create a |ResonanceAudioApi| instance. Caller must
// take ownership of returned instance and destroy it via operator delete.
//
// @param num_channels Number of channels of audio output.
// @param frames_per_buffer Number of frames per buffer.
// @param sample_rate_hz System sample rate.

#ifdef __cplusplus
extern "C" {
#endif

EXPORT_API void* CreateResonanceAudioApi2(size_t num_channels,
                                          size_t frames_per_buffer,
                                          int sample_rate_hz);

// The ResonanceAudioApi library renders high-quality spatial audio. It provides
// methods to binaurally render virtual sound sources with simulated room
// acoustics. In addition, it supports decoding and binaural rendering of
// ambisonic soundfields. Its implementation is single-threaded, thread-safe
// and non-blocking to be able to process raw PCM audio buffers directly on the
// audio thread while receiving parameter updates from the main/render thread.

// Invalid source id that can be used to initialize handler variables during
//  // class construction.
// static const SourceId kInvalidSourceId = -1;

EXPORT_API void DestoryResonanceAudioApi2(void* pv_thiz);

// Renders and outputs an interleaved output buffer in float format.
//
// @param num_frames Size of output buffer in frames.
// @param num_channels Number of channels in output buffer.
// @param buffer_ptr Raw float pointer to audio buffer.
// @return True if a valid output was successfully rendered, false otherwise.
EXPORT_API int FillInterleavedOutputBufferFloat(void* pv_thiz,
                                                size_t num_channels,
                                                size_t num_frames,
                                                float* buffer_ptr);

// Renders and outputs an interleaved output buffer in int16 format.
//
// @param num_channels Number of channels in output buffer.
// @param num_frames Size of output buffer in frames.
// @param buffer_ptr Raw int16 pointer to audio buffer.
// @return True if a valid output was successfully rendered, false otherwise.
EXPORT_API int FillInterleavedOutputBufferInt16(void* pv_thiz,
                                                size_t num_channels,
                                                size_t num_frames,
                                                int16* buffer_ptr);

// Renders and outputs a planar output buffer in float format.
//
// @param num_frames Size of output buffer in frames.
// @param num_channels Number of channels in output buffer.
// @param buffer_ptr Pointer to array of raw float pointers to each channel of
//    the audio buffer.
// @return True if a valid output was successfully rendered, false otherwise.
EXPORT_API int FillPlanarOutputBufferFloat(void* pv_thiz, size_t num_channels,
                                           size_t num_frames,
                                           float* const* buffer_ptr);

// Renders and outputs a planar output buffer in int16 format.
//
// @param num_channels Number of channels in output buffer.
// @param num_frames Size of output buffer in frames.
// @param buffer_ptr Pointer to array of raw int16 pointers to each channel of
//    the audio buffer.
// @return True if a valid output was successfully rendered, false otherwise.
EXPORT_API int FillPlanarOutputBufferInt16(void* pv_thiz, size_t num_channels,
                                           size_t num_frames,
                                           int16* const* buffer_ptr);

// Sets listener's head position.
//
// @param x X coordinate of head position in world space.
// @param y Y coordinate of head position in world space.
// @param z Z coordinate of head position in world space.
EXPORT_API void SetHeadPositionXYZ(void* pv_thiz, float x, float y, float z);

// Sets listener's head rotation.
//
// @param x X component of quaternion.
// @param y Y component of quaternion.
// @param z Z component of quaternion.
// @param w W component of quaternion.
EXPORT_API void SetHeadRotationXYZW(void* pv_thiz, float x, float y, float z,
                                    float w);

// Sets the master volume of the main audio output.
//
// @param volume Master volume (linear) in amplitude in range [0, 1] for
//     attenuation, range [1, inf) for gain boost.
EXPORT_API void SetMasterVolume(void* pv_thiz, float volume);

// Enables the stereo speaker mode. When activated, it disables HRTF-based
// filtering and switches to computationally cheaper stereo-panning. This
// helps to avoid HRTF-based coloring effects when stereo speakers are used
// and reduces computational complexity when headphone-based HRTF filtering is
// not needed. By default the stereo speaker mode is disabled. Note that
// stereo speaker mode overrides the |enable_hrtf| flag in
// |CreateSoundObjectSource|.
//
// @param enabled Flag to enable stereo speaker mode.
EXPORT_API void SetStereoSpeakerMode(void* pv_thiz, int enabled);

// Creates an ambisonic source instance.
//
// @param num_channels Number of input channels.
// @return Id of new ambisonic source.
EXPORT_API SourceId CreateAmbisonicSource(void* pv_thiz, size_t num_channels);

// Creates a stereo non-spatialized source instance, which directly plays back
// mono or stereo audio.
//
// @param num_channels Number of input channels.
// @return Id of new non-spatialized source.
EXPORT_API SourceId CreateStereoSource(void* pv_thiz, size_t num_channels);

// Creates a sound object source instance.
//
// @param rendering_mode Rendering mode which governs quality and performance.
// @return Id of new sound object source.
EXPORT_API SourceId CreateSoundObjectSource(void* pv_thiz,
                                            RenderingMode2 rendering_mode);

// Destroys source instance.
//
// @param source_id Id of source to be destroyed.
EXPORT_API void DestroySource(void* pv_thiz, SourceId id);

// Sets the next audio buffer in interleaved float format to a sound source.
//
// @param source_id Id of sound source.
// @param audio_buffer_ptr Pointer to interleaved float audio buffer.
// @param num_channels Number of channels in interleaved audio buffer.
// @param num_frames Number of frames per channel in interleaved audio buffer.
EXPORT_API void SetInterleavedBufferFloat(void* pv_thiz, SourceId source_id,
                                          const float* audio_buffer_ptr,
                                          size_t num_channels,
                                          size_t num_frames);

// Sets the next audio buffer in interleaved int16 format to a sound source.
//
// @param source_id Id of sound source.
// @param audio_buffer_ptr Pointer to interleaved int16 audio buffer.
// @param num_channels Number of channels in interleaved audio buffer.
// @param num_frames Number of frames per channel in interleaved audio buffer.
EXPORT_API void SetInterleavedBufferInt16(void* pv_thiz, SourceId source_id,
                                          const int16* audio_buffer_ptr,
                                          size_t num_channels,
                                          size_t num_frames);

// Sets the next audio buffer in planar float format to a sound source.
//
// @param source_id Id of sound source.
// @param audio_buffer_ptr Pointer to array of pointers referring to planar
//    audio buffers for each channel.
// @param num_channels Number of planar input audio buffers.
// @param num_frames Number of frames per channel.
EXPORT_API void SetPlanarBufferFloat(void* pv_thiz, SourceId source_id,
                                     const float* const* audio_buffer_ptr,
                                     size_t num_channels, size_t num_frames);

// Sets the next audio buffer in planar int16 format to a sound source.
//
// @param source_id Id of sound source.
// @param audio_buffer_ptr Pointer to array of pointers referring to planar
//    audio buffers for each channel.
// @param num_channels Number of planar input audio buffers.
// @param num_frames Number of frames per channel.
EXPORT_API void SetPlanarBufferInt16(void* pv_thiz, SourceId source_id,
                                     const int16* const* audio_buffer_ptr,
                                     size_t num_channels, size_t num_frames);

// Sets the given source's distance attenuation value explicitly. The distance
// rolloff model of the source must be set to |DistanceRolloffModel::kNone|
// for the set value to take effect.
//
// @param source_id Id of source.
// @param distance_attenuation Distance attenuation value.
EXPORT_API void SetSourceDistanceAttenuation(void* pv_thiz, SourceId source_id,
                                             float distance_attenuation);

// Sets the given source's distance attenuation method with minimum and
// maximum distances. Maximum distance must be greater than the minimum
// distance for the method to be set.
//
// @param source_id Id of source.
// @param rolloff Linear or logarithmic distance rolloff models.
// @param min_distance Minimum distance to apply distance attenuation method.
// @param max_distance Maximum distance to apply distance attenuation method.
EXPORT_API void SetSourceDistanceModel(void* pv_thiz, SourceId source_id,
                                       DistanceRolloffModel2 rolloff,
                                       float min_distance, float max_distance);

// Sets the given source's position. Note that, the given position for an
// ambisonic source is only used to determine the corresponding room effects
// to be applied.
//
// @param source_id Id of source.
// @param x X coordinate of source position in world space.
// @param y Y coordinate of source position in world space.
// @param z Z coordinate of source position in world space.
EXPORT_API void SetSourcePosition(void* pv_thiz, SourceId source_id, float x,
                                  float y, float z);

// Sets the room effects contribution for the given source.
//
// @param source_id Id of source.
// @param room_effects_gain Linear room effects volume in amplitude in range
//     [0, 1] for attenuation, range [1, inf) for gain boost.
EXPORT_API void SetSourceRoomEffectsGain(void* pv_thiz, SourceId source_id,
                                         float room_effects_gain);

// Sets the given source's rotation.
//
// @param source_id Id of source.
// @param x X component of quaternion.
// @param y Y component of quaternion.
// @param z Z component of quaternion.
// @param w W component of quaternion.
EXPORT_API void SetSourceRotation(void* pv_thiz, SourceId source_id, float x,
                                  float y, float z, float w);

// Sets the given source's volume.
//
// @param source_id Id of source.
// @param volume Linear source volume in amplitude in range [0, 1] for
//     attenuation, range [1, inf) for gain boost.
EXPORT_API void SetSourceVolume(void* pv_thiz, SourceId source_id,
                                float volume);

// Sets the given sound object source's directivity.
//
// @param sound_object_source_id Id of sound object source.
// @param alpha Weighting balance between figure of eight pattern and circular
//     pattern for source emission in range [0, 1]. A value of 0.5 results in
//     a cardioid pattern.
// @param order Order applied to computed directivity. Higher values will
//     result in narrower and sharper directivity patterns. Range [1, inf).
EXPORT_API void SetSoundObjectDirectivity(void* pv_thiz,
                                          SourceId sound_object_source_id,
                                          float alpha, float order);

// Sets the listener's directivity with respect to the given sound object.
// This method could be used to simulate an angular rolloff in terms of the
// listener's orientation, given the polar pickup pattern with |alpha| and
// |order|.
//
// @param sound_object_source_id Id of sound object source.
// @param alpha Weighting balance between figure of eight pattern and circular
//     pattern for listener's pickup in range [0, 1]. A value of 0.5 results
//     in a cardioid pattern.
// @param order Order applied to computed pickup pattern. Higher values will
//     result in narrower and sharper pickup patterns. Range [1, inf).
EXPORT_API void SetSoundObjectListenerDirectivity(
    void* pv_thiz, SourceId sound_object_source_id, float alpha, float order);

// Sets the gain (linear) of the near field effect.
//
// @param sound_object_source_id Id of sound object source.
// @param gain Gain of the near field effect. Range [0, 9] (corresponding to
//     approx. (-Inf, +20dB]).
EXPORT_API void SetSoundObjectNearFieldEffectGain(
    void* pv_thiz, SourceId sound_object_source_id, float gain);

// Sets the given sound object source's occlusion intensity.
//
// @param sound_object_source_id Id of sound object source.
// @param intensity Number of occlusions occurred for the object. The value
//     can be set to fractional for partial occlusions. Range [0, inf).
EXPORT_API void SetSoundObjectOcclusionIntensity(
    void* pv_thiz, SourceId sound_object_source_id, float intensity);

// Sets the given sound object source's spread.
//
// @param sound_object_source_id Id of sound object source.
// @param spread_deg Spread in degrees.
EXPORT_API void SetSoundObjectSpread(void* pv_thiz,
                                     SourceId sound_object_source_id,
                                     float spread_deg);

// Turns on/off the reflections and reverberation.
EXPORT_API void EnableRoomEffects(void* pv_thiz, int enable);

// Sets the early reflection properties of the environment.
//
// @param reflection_properties Reflection properties.
EXPORT_API void SetReflectionProperties(
    void* pv_thiz, const ReflectionProperties2* reflection_properties);

// Sets the late reverberation properties of the environment.
//
// @param reverb_properties Reverb properties.
EXPORT_API void SetReverbProperties(void* pv_thiz,
                                    const ReverbProperties2* reverb_properties);

#ifdef __cplusplus
}
#endif

#endif  // RESONANCE_AUDIO_API_RESONANCE_AUDIO_API2_H_
