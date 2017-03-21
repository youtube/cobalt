/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "bindings/core/v8/DictionaryHelperForBindings.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8Gamepad.h"
#include "bindings/modules/v8/V8Headers.h"
#include "bindings/modules/v8/V8MIDIPort.h"
#include "bindings/modules/v8/V8MediaStream.h"
#include "bindings/modules/v8/V8SpeechRecognitionResult.h"
#include "bindings/modules/v8/V8SpeechRecognitionResultList.h"
#include "modules/gamepad/Gamepad.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "modules/speech/SpeechRecognitionResultList.h"

namespace blink {

template <>
struct DictionaryHelperTraits<MIDIPort> {
    typedef V8MIDIPort type;
};

template <>
struct DictionaryHelperTraits<SpeechRecognitionResultList> {
    typedef V8SpeechRecognitionResultList type;
};

template <>
struct DictionaryHelperTraits<Gamepad> {
    typedef V8Gamepad type;
};

template <>
struct DictionaryHelperTraits<MediaStream> {
    typedef V8MediaStream type;
};

template <>
struct DictionaryHelperTraits<Headers> {
    typedef V8Headers type;
};

template bool DictionaryHelper::get(const Dictionary&, const String& key, Member<MIDIPort>& value);
template bool DictionaryHelper::get(const Dictionary&, const String& key, Member<SpeechRecognitionResultList>& value);
template bool DictionaryHelper::get(const Dictionary&, const String& key, Member<Gamepad>& value);
template bool DictionaryHelper::get(const Dictionary&, const String& key, Member<MediaStream>& value);
template bool DictionaryHelper::get(const Dictionary&, const String& key, Member<Headers>& value);

template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, Member<MIDIPort>& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, Member<SpeechRecognitionResultList>& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, Member<Gamepad>& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, Member<MediaStream>& value);
template bool DictionaryHelper::convert(const Dictionary&, Dictionary::ConversionContext&, const String& key, Member<Headers>& value);

} // namespace blink
