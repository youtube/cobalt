/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "third_party/blink/public/web/web_dom_media_stream_track.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

namespace blink {

WebDOMMediaStreamTrack::WebDOMMediaStreamTrack(MediaStreamTrack* track)
    : private_(track) {}

WebDOMMediaStreamTrack WebDOMMediaStreamTrack::FromV8Value(
    v8::Local<v8::Value> value) {
  if (V8MediaStreamTrack::HasInstance(value, v8::Isolate::GetCurrent())) {
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
    return WebDOMMediaStreamTrack(V8MediaStreamTrack::ToImpl(object));
  }
  return WebDOMMediaStreamTrack(nullptr);
}

void WebDOMMediaStreamTrack::Reset() {
  private_.Reset();
}

void WebDOMMediaStreamTrack::Assign(const WebDOMMediaStreamTrack& b) {
  private_ = b.private_;
}

WebMediaStreamTrack WebDOMMediaStreamTrack::Component() const {
  return WebMediaStreamTrack(private_->Component());
}

}  // namespace blink
