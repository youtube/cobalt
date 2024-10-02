/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_

#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_inspector_node.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

class AudioContext;
class AudioNodeOptions;
class ExceptionState;

class MediaStreamAudioDestinationNode final : public AudioBasicInspectorNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStreamAudioDestinationNode* Create(AudioContext&,
                                                 uint32_t number_of_channels,
                                                 ExceptionState&);
  static MediaStreamAudioDestinationNode* Create(AudioContext*,
                                                 const AudioNodeOptions*,
                                                 ExceptionState&);

  MediaStreamAudioDestinationNode(AudioContext&, uint32_t number_of_channels);

  MediaStream* stream() const { return stream_; }
  MediaStreamSource* source() const { return source_; }

  void Trace(Visitor*) const final;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  const Member<MediaStreamSource> source_;
  const Member<MediaStream> stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_
