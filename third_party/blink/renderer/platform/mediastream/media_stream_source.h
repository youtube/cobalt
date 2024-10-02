/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_SOURCE_H_

#include <memory>
#include <utility>

#include "base/synchronization/lock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_source.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/display/types/display_constants.h"

namespace blink {

class AudioBus;
class WebAudioDestinationConsumer;

// GarbageCollected wrapper of a WebPlatformMediaStreamSource, which acts as a
// source backing one or more MediaStreamTracks.
class PLATFORM_EXPORT MediaStreamSource final
    : public GarbageCollected<MediaStreamSource> {
  USING_PRE_FINALIZER(MediaStreamSource, Dispose);

 public:
  class PLATFORM_EXPORT Observer : public GarbageCollectedMixin {
   public:
    virtual ~Observer() = default;
    virtual void SourceChangedState() = 0;
    virtual void SourceChangedCaptureConfiguration() = 0;
    virtual void SourceChangedCaptureHandle() = 0;
  };

  enum StreamType { kTypeAudio, kTypeVideo };

  enum ReadyState {
    kReadyStateLive = 0,
    kReadyStateMuted = 1,
    kReadyStateEnded = 2
  };

  enum class EchoCancellationMode { kDisabled, kBrowser, kAec3, kSystem };

  MediaStreamSource(
      const String& id,
      StreamType type,
      const String& name,
      bool remote,
      std::unique_ptr<WebPlatformMediaStreamSource> platform_source,
      ReadyState state = kReadyStateLive,
      bool requires_consumer = false);

  MediaStreamSource(
      const String& id,
      int64_t display_id,
      StreamType type,
      const String& name,
      bool remote,
      std::unique_ptr<WebPlatformMediaStreamSource> platform_source,
      ReadyState state = kReadyStateLive,
      bool requires_consumer = false);

  const String& Id() const { return id_; }
  int64_t GetDisplayId() const { return display_id_; }
  StreamType GetType() const { return type_; }
  const String& GetName() const { return name_; }
  bool Remote() const { return remote_; }

  void SetGroupId(const String& group_id);
  const String& GroupId() { return group_id_; }

  void SetReadyState(ReadyState);
  ReadyState GetReadyState() const { return ready_state_; }

  void AddObserver(Observer*);

  WebPlatformMediaStreamSource* GetPlatformSource() const {
    return platform_source_.get();
  }

  void SetAudioProcessingProperties(EchoCancellationMode echo_cancellation_mode,
                                    bool auto_gain_control,
                                    bool noise_supression);

  void GetSettings(MediaStreamTrackPlatform::Settings&);

  struct Capabilities {
    // Vector is used to store an optional range for the below numeric
    // fields. All of them should have 0 or 2 values representing min/max.
    Vector<uint32_t> width;
    Vector<uint32_t> height;
    Vector<double> aspect_ratio;
    Vector<double> frame_rate;
    Vector<bool> echo_cancellation;
    Vector<String> echo_cancellation_type;
    Vector<bool> auto_gain_control;
    Vector<bool> noise_suppression;
    Vector<int32_t> sample_size;
    Vector<int32_t> channel_count;
    Vector<int32_t> sample_rate;
    Vector<double> latency;

    MediaStreamTrackPlatform::FacingMode facing_mode =
        MediaStreamTrackPlatform::FacingMode::kNone;
    String device_id;
    String group_id;
  };

  const Capabilities& GetCapabilities() { return capabilities_; }
  void SetCapabilities(const Capabilities& capabilities) {
    capabilities_ = capabilities;
  }

  void SetAudioFormat(int number_of_channels, float sample_rate);
  void ConsumeAudio(AudioBus*, int number_of_frames);

  // Only used if this is a WebAudio source.
  // The WebAudioDestinationConsumer is not owned, and has to be disposed of
  // separately after calling removeAudioConsumer.
  bool RequiresAudioConsumer() const { return requires_consumer_; }
  void SetAudioConsumer(WebAudioDestinationConsumer*);
  bool RemoveAudioConsumer();

  void OnDeviceCaptureConfigurationChange(const MediaStreamDevice& device);
  void OnDeviceCaptureHandleChange(const MediaStreamDevice& device);

  void Trace(Visitor*) const;

  void Dispose();

 private:
  class PLATFORM_EXPORT ConsumerWrapper final {
    USING_FAST_MALLOC(ConsumerWrapper);

   public:
    explicit ConsumerWrapper(WebAudioDestinationConsumer* consumer);

    void SetFormat(int number_of_channels, float sample_rate);
    void ConsumeAudio(AudioBus* bus, int number_of_frames);

    // m_consumer is not owned by this class.
    WebAudioDestinationConsumer* consumer_;
    // bus_vector_ must only be used in ConsumeAudio. The only reason it's a
    // member variable is to not have to reallocate it for each call.
    Vector<const float*> bus_vector_;
  };

  // The ID of this MediaStreamSource object itself.
  String id_;
  // If this MediaStreamSource object is associated with a display,
  // then `display_id_` holds the display's own ID.
  // Otherwise, display::kInvalidDisplayId.
  // This attribute is currently only set on ChromeOS.
  int64_t display_id_ = display::kInvalidDisplayId;
  StreamType type_;
  String name_;
  String group_id_;
  bool remote_;
  ReadyState ready_state_;
  bool requires_consumer_;
  HeapHashSet<WeakMember<Observer>> observers_;
  base::Lock audio_consumer_lock_;
  std::unique_ptr<ConsumerWrapper> audio_consumer_
      GUARDED_BY(audio_consumer_lock_);
  std::unique_ptr<WebPlatformMediaStreamSource> platform_source_;
  Capabilities capabilities_;
  absl::optional<EchoCancellationMode> echo_cancellation_mode_;
  absl::optional<bool> auto_gain_control_;
  absl::optional<bool> noise_supression_;
};

typedef HeapVector<Member<MediaStreamSource>> MediaStreamSourceVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_SOURCE_H_
