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

#include <cstddef>  // size_t declaration.
#include <cstdint>  // int16_t declaration.
#define DLL_EXPORTS

#include "resonance_audio_api.h"
#include "resonance_audio_api2.h"

namespace vraudio {


    extern "C" EXPORT_API void* CreateResonanceAudioApi2(
        size_t num_channels, size_t frames_per_buffer, int sample_rate_hz) {
        return (void*)CreateResonanceAudioApi(num_channels, frames_per_buffer,
            sample_rate_hz);
    }


    // Sound object / ambisonic source identifier.
    typedef int SourceId;

    // Invalid source id that can be used to initialize handler variables during
    // class construction.
    //static const SourceId ` = -1;

    extern "C" EXPORT_API void DestoryResonanceAudioApi2(void* pv_thiz)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        delete thiz;
    }

    // Renders and outputs an interleaved output buffer in float format.
    //
    // @param num_frames Size of output buffer in frames.
    // @param num_channels Number of channels in output buffer.
    // @param buffer_ptr Raw float pointer to audio buffer.
    // @return True if a valid output was successfully rendered, false otherwise.
    extern "C" EXPORT_API int FillInterleavedOutputBufferFloat(void* pv_thiz, size_t num_channels,
        size_t num_frames,
        float* buffer_ptr)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (int)(thiz->FillInterleavedOutputBuffer(num_channels, num_frames, buffer_ptr));
    }


    // Renders and outputs an interleaved output buffer in int16 format.
    //
    // @param num_channels Number of channels in output buffer.
    // @param num_frames Size of output buffer in frames.
    // @param buffer_ptr Raw int16 pointer to audio buffer.
    // @return True if a valid output was successfully rendered, false otherwise.
    extern "C" EXPORT_API int FillInterleavedOutputBufferInt16(void* pv_thiz, size_t num_channels,
        size_t num_frames,
        int16 * buffer_ptr)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (int)(thiz->FillInterleavedOutputBuffer(num_channels, num_frames, buffer_ptr));
    }

    // Renders and outputs a planar output buffer in float format.
    //
    // @param num_frames Size of output buffer in frames.
    // @param num_channels Number of channels in output buffer.
    // @param buffer_ptr Pointer to array of raw float pointers to each channel of
    //    the audio buffer.
    // @return True if a valid output was successfully rendered, false otherwise.
    extern "C" EXPORT_API int FillPlanarOutputBufferFloat(void* pv_thiz, size_t num_channels, size_t num_frames,
        float* const* buffer_ptr)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (int)(thiz->FillPlanarOutputBuffer(num_channels, num_frames, buffer_ptr));
    }
    // Renders and outputs a planar output buffer in int16 format.
    //
    // @param num_channels Number of channels in output buffer.
    // @param num_frames Size of output buffer in frames.
    // @param buffer_ptr Pointer to array of raw int16 pointers to each channel of
    //    the audio buffer.
    // @return True if a valid output was successfully rendered, false otherwise.
    extern "C" EXPORT_API int FillPlanarOutputBufferInt16(void* pv_thiz, size_t num_channels, size_t num_frames,
        int16* const* buffer_ptr)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (int)(thiz->FillPlanarOutputBuffer(num_channels, num_frames, buffer_ptr));
    }
    // Sets listener's head position.
    //
    // @param x X coordinate of head position in world space.
    // @param y Y coordinate of head position in world space.
    // @param z Z coordinate of head position in world space.
    extern "C" EXPORT_API void SetHeadPositionXYZ(void* pv_thiz, float x, float y, float z)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetHeadPosition(x, y, z));
    }
    // Sets listener's head rotation.
    //
    // @param x X component of quaternion.
    // @param y Y component of quaternion.
    // @param z Z component of quaternion.
    // @param w W component of quaternion.
    extern "C" EXPORT_API void SetHeadRotationXYZW(void* pv_thiz, float x, float y, float z, float w)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetHeadRotation(x, y, z, w));
    }
    // Sets the master volume of the main audio output.
    //
    // @param volume Master volume (linear) in amplitude in range [0, 1] for
    //     attenuation, range [1, inf) for gain boost.
    extern "C" EXPORT_API void SetMasterVolume(void* pv_thiz, float volume)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetMasterVolume(volume));
    }
    // Enables the stereo speaker mode. When activated, it disables HRTF-based
    // filtering and switches to computationally cheaper stereo-panning. This
    // helps to avoid HRTF-based coloring effects when stereo speakers are used
    // and reduces computational complexity when headphone-based HRTF filtering is
    // not needed. By default the stereo speaker mode is disabled. Note that
    // stereo speaker mode overrides the |enable_hrtf| flag in
    // |CreateSoundObjectSource|.
    //
    // @param enabled Flag to enable stereo speaker mode.
    extern "C" EXPORT_API void SetStereoSpeakerMode(void* pv_thiz, int enabled)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetStereoSpeakerMode((bool)enabled));
    }
    // Creates an ambisonic source instance.
    //
    // @param num_channels Number of input channels.
    // @return Id of new ambisonic source.
    extern "C" EXPORT_API SourceId CreateAmbisonicSource(void* pv_thiz, size_t num_channels)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->CreateAmbisonicSource(num_channels));
    }
    // Creates a stereo non-spatialized source instance, which directly plays back
    // mono or stereo audio.
    //
    // @param num_channels Number of input channels.
    // @return Id of new non-spatialized source.
    extern "C" EXPORT_API SourceId CreateStereoSource(void* pv_thiz, size_t num_channels)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->CreateStereoSource(num_channels));
    }
    // Creates a sound object source instance.
    //
    // @param rendering_mode Rendering mode which governs quality and performance.
    // @return Id of new sound object source.
    extern "C" EXPORT_API SourceId CreateSoundObjectSource(void* pv_thiz, RenderingMode2 rendering_mode)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->CreateSoundObjectSource((RenderingMode)rendering_mode));
    }
    // Destroys source instance.
    //
    // @param source_id Id of source to be destroyed.
    extern "C" EXPORT_API void DestroySource(void* pv_thiz, SourceId id)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->DestroySource(id));
    }
    // Sets the next audio buffer in interleaved float format to a sound source.
    //
    // @param source_id Id of sound source.
    // @param audio_buffer_ptr Pointer to interleaved float audio buffer.
    // @param num_channels Number of channels in interleaved audio buffer.
    // @param num_frames Number of frames per channel in interleaved audio buffer.
    extern "C" EXPORT_API void SetInterleavedBufferFloat(void* pv_thiz, SourceId source_id,
        const float* audio_buffer_ptr,
        size_t num_channels, size_t num_frames)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetInterleavedBuffer(source_id, audio_buffer_ptr, num_channels, num_frames));
    }
    // Sets the next audio buffer in interleaved int16 format to a sound source.
    //
    // @param source_id Id of sound source.
    // @param audio_buffer_ptr Pointer to interleaved int16 audio buffer.
    // @param num_channels Number of channels in interleaved audio buffer.
    // @param num_frames Number of frames per channel in interleaved audio buffer.
    extern "C" EXPORT_API void SetInterleavedBufferInt16(void* pv_thiz, SourceId source_id,
        const int16 * audio_buffer_ptr,
        size_t num_channels, size_t num_frames)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetInterleavedBuffer(source_id, audio_buffer_ptr, num_channels, num_frames));
    }
    // Sets the next audio buffer in planar float format to a sound source.
    //
    // @param source_id Id of sound source.
    // @param audio_buffer_ptr Pointer to array of pointers referring to planar
    //    audio buffers for each channel.
    // @param num_channels Number of planar input audio buffers.
    // @param num_frames Number of frames per channel.
    extern "C" EXPORT_API void SetPlanarBufferFloat(void* pv_thiz, SourceId source_id,
        const float* const* audio_buffer_ptr,
        size_t num_channels, size_t num_frames)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetPlanarBuffer(source_id, audio_buffer_ptr, num_channels, num_frames));
    }
    // Sets the next audio buffer in planar int16 format to a sound source.
    //
    // @param source_id Id of sound source.
    // @param audio_buffer_ptr Pointer to array of pointers referring to planar
    //    audio buffers for each channel.
    // @param num_channels Number of planar input audio buffers.
    // @param num_frames Number of frames per channel.
    extern "C" EXPORT_API void SetPlanarBufferInt16(void* pv_thiz, SourceId source_id,
        const int16* const* audio_buffer_ptr,
        size_t num_channels, size_t num_frames)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetPlanarBuffer(source_id, audio_buffer_ptr, num_channels, num_frames));
    }
    // Sets the given source's distance attenuation value explicitly. The distance
    // rolloff model of the source must be set to |DistanceRolloffModel::kNone|
    // for the set value to take effect.
    //
    // @param source_id Id of source.
    // @param distance_attenuation Distance attenuation value.
    extern "C" EXPORT_API void SetSourceDistanceAttenuation(void* pv_thiz, SourceId source_id,
        float distance_attenuation)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSourceDistanceAttenuation(source_id, distance_attenuation));
    }
    // Sets the given source's distance attenuation method with minimum and
    // maximum distances. Maximum distance must be greater than the minimum
    // distance for the method to be set.
    //
    // @param source_id Id of source.
    // @param rolloff Linear or logarithmic distance rolloff models.
    // @param min_distance Minimum distance to apply distance attenuation method.
    // @param max_distance Maximum distance to apply distance attenuation method.
    extern "C" EXPORT_API void SetSourceDistanceModel(void* pv_thiz, SourceId source_id,
        DistanceRolloffModel2 rolloff,
        float min_distance,
        float max_distance)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSourceDistanceModel(source_id, (DistanceRolloffModel)rolloff, min_distance, max_distance));
    }
    // Sets the given source's position. Note that, the given position for an
    // ambisonic source is only used to determine the corresponding room effects
    // to be applied.
    //
    // @param source_id Id of source.
    // @param x X coordinate of source position in world space.
    // @param y Y coordinate of source position in world space.
    // @param z Z coordinate of source position in world space.
    extern "C" EXPORT_API void SetSourcePosition(void* pv_thiz, SourceId source_id, float x, float y,
        float z)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSourcePosition(source_id, x, y, z));
    }
    // Sets the room effects contribution for the given source.
    //
    // @param source_id Id of source.
    // @param room_effects_gain Linear room effects volume in amplitude in range
    //     [0, 1] for attenuation, range [1, inf) for gain boost.
    extern "C" EXPORT_API void SetSourceRoomEffectsGain(void* pv_thiz, SourceId source_id,
        float room_effects_gain)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSourceRoomEffectsGain(source_id, room_effects_gain));
    }
    // Sets the given source's rotation.
    //
    // @param source_id Id of source.
    // @param x X component of quaternion.
    // @param y Y component of quaternion.
    // @param z Z component of quaternion.
    // @param w W component of quaternion.
    extern "C" EXPORT_API void SetSourceRotation(void* pv_thiz, SourceId source_id, float x, float y, float z,
        float w)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSourceRotation(source_id, x, y, z, w));
    }
    // Sets the given source's volume.
    //
    // @param source_id Id of source.
    // @param volume Linear source volume in amplitude in range [0, 1] for
    //     attenuation, range [1, inf) for gain boost.
    extern "C" EXPORT_API void SetSourceVolume(void* pv_thiz, SourceId source_id, float volume)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSourceVolume(source_id, volume));
    }
    // Sets the given sound object source's directivity.
    //
    // @param sound_object_source_id Id of sound object source.
    // @param alpha Weighting balance between figure of eight pattern and circular
    //     pattern for source emission in range [0, 1]. A value of 0.5 results in
    //     a cardioid pattern.
    // @param order Order applied to computed directivity. Higher values will
    //     result in narrower and sharper directivity patterns. Range [1, inf).
    extern "C" EXPORT_API void SetSoundObjectDirectivity(void* pv_thiz, SourceId sound_object_source_id,
        float alpha, float order)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSoundObjectDirectivity(sound_object_source_id, alpha, order));
    }
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
    extern "C" EXPORT_API void SetSoundObjectListenerDirectivity(void* pv_thiz,
        SourceId sound_object_source_id, float alpha, float order)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSoundObjectListenerDirectivity(sound_object_source_id, alpha, order));
    }
    // Sets the gain (linear) of the near field effect.
    //
    // @param sound_object_source_id Id of sound object source.
    // @param gain Gain of the near field effect. Range [0, 9] (corresponding to
    //     approx. (-Inf, +20dB]).
    extern "C" EXPORT_API void SetSoundObjectNearFieldEffectGain(void* pv_thiz,
        SourceId sound_object_source_id, float gain)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSoundObjectNearFieldEffectGain(sound_object_source_id, gain));
    }
    // Sets the given sound object source's occlusion intensity.
    //
    // @param sound_object_source_id Id of sound object source.
    // @param intensity Number of occlusions occurred for the object. The value
    //     can be set to fractional for partial occlusions. Range [0, inf).
    extern "C" EXPORT_API void SetSoundObjectOcclusionIntensity(void* pv_thiz, SourceId sound_object_source_id,
        float intensity)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSoundObjectOcclusionIntensity(sound_object_source_id, intensity));
    }
    // Sets the given sound object source's spread.
    //
    // @param sound_object_source_id Id of sound object source.
    // @param spread_deg Spread in degrees.
    extern "C" EXPORT_API void SetSoundObjectSpread(void* pv_thiz, SourceId sound_object_source_id,
        float spread_deg)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetSoundObjectSpread(sound_object_source_id, spread_deg));
    }
    // Turns on/off the reflections and reverberation.
    extern "C" EXPORT_API void EnableRoomEffects(void* pv_thiz, int enable)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->EnableRoomEffects((bool)enable));
    }
    // Sets the early reflection properties of the environment.
    //
    // @param reflection_properties Reflection properties.
    extern "C" EXPORT_API void SetReflectionProperties(void* pv_thiz,
        const ReflectionProperties2 * reflection_properties)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetReflectionProperties((ReflectionProperties&)*reflection_properties));
    }
    // Sets the late reverberation properties of the environment.
    //
    // @param reverb_properties Reverb properties.
    extern "C" EXPORT_API void SetReverbProperties(void* pv_thiz,
        const ReverbProperties2 * reverb_properties)
    {
        ResonanceAudioApi* thiz = (ResonanceAudioApi*)pv_thiz;
        return (thiz->SetReverbProperties((ReverbProperties&)*reverb_properties));
    }
}  // namespace vraudio
