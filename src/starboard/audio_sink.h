// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// An interface to output raw audio data.

#ifndef STARBOARD_AUDIO_SINK_H_
#define STARBOARD_AUDIO_SINK_H_

#include "starboard/configuration.h"

#include "starboard/export.h"
#include "starboard/media.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// An array of frame buffers.  For interleaved audio streams, there will be
// only one element in the array.  For planar audio streams, the number of
// elements in the array equal to the number of channels.
typedef void** SbAudioSinkFrameBuffers;

// An opaque handle to an implementation-private structure representing an
// audio sink.
typedef struct SbAudioSinkPrivate* SbAudioSink;

// Callback being called periodically to retrieve the status of the audio
// source.  The first two output parameters indicating the filling level of the
// audio frame buffer passed to SbAudioSinkCreate as |frame_buffers|.  As
// |frame_buffers| is a circular buffer, |offset_in_frames| can be used to
// calculate the number of continuous frames towards the end of the buffer.
// All parameters except |context| cannot be NULL.
// Note that this function only reports the status of the source, it doesn't
// remove audio data from the source frame buffer.
typedef void (*SbAudioSinkUpdateSourceStatusFunc)(int* frames_in_buffer,
                                                  int* offset_in_frames,
                                                  bool* is_playing,
                                                  bool* is_eos_reached,
                                                  void* context);

// Callback used to report frames consumed.  The consumed frames will be
// removed from the source frame buffer to free space for new audio frames.
typedef void (*SbAudioSinkConsumeFramesFunc)(int frames_consumed,
                                             void* context);

// Well-defined value for an invalid audio sink.
#define kSbAudioSinkInvalid ((SbAudioSink)NULL)

// --- Functions -------------------------------------------------------------

// Returns whether the given audio sink handle is valid.
SB_EXPORT bool SbAudioSinkIsValid(SbAudioSink audio_sink);

// Creates an audio sink for the specified |channels| and
// |sampling_frequency_hz|, acquiring all resources needed to operate it, and
// returning an opaque handle to it.
//
// |frame_buffers| is an array of pointers to sample data.  If the sink is
// operating in interleaved mode, the array contains only one element, which is
// an array containing |frames_per_channel| * |channels| samples.  If the sink
// is operating in planar mode, the number of elements in the array will be the
// same as |channels|, each of the elements will be an array of
// |frames_per_channel| samples.  The caller has to ensure that |frame_buffers|
// is valid until SbAudioSinkDestroy is called.
//
// |update_source_status_func| cannot be NULL.  The audio sink will call it on
// an internal thread to query the status of the source.
//
// |consume_frames_func| cannot be NULL.  The audio sink will call it on an
// internal thread to report consumed frames.
//
// |context| will be passed back into all callbacks, and is generally used to
// point at a class or struct that contains state associated with the audio
// sink.
//
// The audio sink will start to call |update_source_status_func| immediately
// after SbAudioSinkCreate is called, even before it returns.  The caller has
// to ensure that the above callbacks returns meaningful values in this case.
//
// If the particular platform doesn't support the requested audio sink, the
// function returns kSbAudioSinkInvalid without calling any of the callbacks.
SB_EXPORT SbAudioSink
SbAudioSinkCreate(int channels,
                  int sampling_frequency_hz,
                  SbMediaAudioSampleType audio_sample_type,
                  SbMediaAudioFrameStorageType audio_frame_storage_type,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                  SbAudioSinkConsumeFramesFunc consume_frames_func,
                  void* context);

// Destroys |audio_sink|, freeing all associated resources.  It will wait until
// all callbacks in progress is finished before returning.  Upon returning of
// this function, no callbacks passed into SbAudioSinkCreate will be called
// further.  This function can be called on any thread but cannot be called
// within any of the callbacks passed into SbAudioSinkCreate.
// It is not allowed to pass |audio_sink| into any other SbAudioSink function
// once SbAudioSinkDestroy has been called on it.
SB_EXPORT void SbAudioSinkDestroy(SbAudioSink audio_sink);

// Returns the maximum channel supported on the platform.
SB_EXPORT int SbAudioSinkGetMaxChannels();

// Returns the nearest supported sample rate of |sampling_frequency_hz|.  On
// platforms that don't support all sample rates, it is the caller's
// responsibility to resample the audio frames into the supported sample rate
// returned by this function.
SB_EXPORT int SbAudioSinkGetNearestSupportedSampleFrequency(
    int sampling_frequency_hz);

// Returns true if the particular SbMediaAudioSampleType is supported on this
// platform.
SB_EXPORT bool SbAudioSinkIsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type);

// Returns true if the particular SbMediaAudioFrameStorageType is supported on
// this platform.
SB_EXPORT bool SbAudioSinkIsAudioFrameStorageTypeSupported(
    SbMediaAudioFrameStorageType audio_frame_storage_type);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_AUDIO_SINK_H_
