// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_V8_SCRIPT_VALUE_DESERIALIZER_FOR_MODULES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_V8_SCRIPT_VALUE_DESERIALIZER_FOR_MODULES_H_

#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_deserializer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AudioData;
class CropTarget;
class CryptoKey;
class EncodedAudioChunk;
class EncodedVideoChunk;
class FileSystemHandle;
class MediaSourceHandleImpl;
class RTCEncodedAudioFrame;
class RTCEncodedVideoFrame;
class VideoFrame;

// Extends V8ScriptValueSerializer with support for modules/ types.
class MODULES_EXPORT V8ScriptValueDeserializerForModules final
    : public V8ScriptValueDeserializer {
 public:
  // TODO(jbroman): This should just be:
  // using V8ScriptValueDeserializer::V8ScriptValueDeserializer;
  // Unfortunately, MSVC 2015 emits C2248, claiming that it cannot access its
  // own private members. Until it's gone, we write the constructors by hand.
  V8ScriptValueDeserializerForModules(ScriptState* script_state,
                                      UnpackedSerializedScriptValue* unpacked,
                                      const Options& options = Options())
      : V8ScriptValueDeserializer(std::move(script_state), unpacked, options) {}
  V8ScriptValueDeserializerForModules(
      ScriptState* script_state,
      scoped_refptr<SerializedScriptValue> value,
      const Options& options = Options())
      : V8ScriptValueDeserializer(script_state, std::move(value), options) {}

  static bool ExecutionContextExposesInterface(ExecutionContext*,
                                               SerializationTag interface_tag);

 protected:
  ScriptWrappable* ReadDOMObject(SerializationTag, ExceptionState&) override;

 private:
  bool ReadOneByte(uint8_t* byte) {
    const void* data;
    if (!ReadRawBytes(1, &data))
      return false;
    *byte = *reinterpret_cast<const uint8_t*>(data);
    return true;
  }
  CryptoKey* ReadCryptoKey();
  FileSystemHandle* ReadFileSystemHandle(SerializationTag tag);
  RTCEncodedAudioFrame* ReadRTCEncodedAudioFrame();
  RTCEncodedVideoFrame* ReadRTCEncodedVideoFrame();
  AudioData* ReadAudioData();
  VideoFrame* ReadVideoFrame();
  EncodedAudioChunk* ReadEncodedAudioChunk();
  EncodedVideoChunk* ReadEncodedVideoChunk();
  MediaStreamTrack* ReadMediaStreamTrack();
  CropTarget* ReadCropTarget();
  MediaSourceHandleImpl* ReadMediaSourceHandle();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_SERIALIZATION_V8_SCRIPT_VALUE_DESERIALIZER_FOR_MODULES_H_
