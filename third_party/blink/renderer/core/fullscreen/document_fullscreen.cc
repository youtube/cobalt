/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
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

#include "third_party/blink/renderer/core/fullscreen/document_fullscreen.h"

#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"

namespace blink {

bool DocumentFullscreen::fullscreenEnabled(Document& document) {
  return Fullscreen::FullscreenEnabled(document);
}

Element* DocumentFullscreen::fullscreenElement(Document& document) {
  return Fullscreen::FullscreenElementForBindingFrom(document);
}

ScriptPromise DocumentFullscreen::exitFullscreen(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  return Fullscreen::ExitFullscreen(document, script_state, &exception_state);
}

void DocumentFullscreen::webkitExitFullscreen(Document& document) {
  ScriptPromise promise = Fullscreen::ExitFullscreen(document);
  DCHECK(promise.IsEmpty());
}

}  // namespace blink
