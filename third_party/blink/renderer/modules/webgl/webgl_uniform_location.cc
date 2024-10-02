/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_uniform_location.h"

namespace blink {

WebGLUniformLocation::WebGLUniformLocation(WebGLProgram* program,
                                           GLint location)
    : program_(program), location_(location) {
  DCHECK(program_);
  link_count_ = program_->LinkCount();
}

WebGLProgram* WebGLUniformLocation::Program() const {
  // If the program has been linked again, then this UniformLocation is no
  // longer valid.
  if (program_->LinkCount() != link_count_)
    return nullptr;
  return program_;
}

GLint WebGLUniformLocation::Location() const {
  // If the program has been linked again, then this UniformLocation is no
  // longer valid.
  DCHECK_EQ(program_->LinkCount(), link_count_);
  return location_;
}

void WebGLUniformLocation::Trace(Visitor* visitor) const {
  visitor->Trace(program_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
