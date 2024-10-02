/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
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

#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "base/notreached.h"

namespace WTF {

TextCodec::~TextCodec() = default;

uint32_t TextCodec::GetUnencodableReplacement(
    unsigned code_point,
    UnencodableHandling handling,
    UnencodableReplacementArray replacement) {
  switch (handling) {
    case kEntitiesForUnencodables:
      snprintf(replacement, sizeof(UnencodableReplacementArray), "&#%u;",
               code_point);
      return static_cast<uint32_t>(strlen(replacement));
    case kURLEncodedEntitiesForUnencodables:
      snprintf(replacement, sizeof(UnencodableReplacementArray),
               "%%26%%23%u%%3B", code_point);
      return static_cast<uint32_t>(strlen(replacement));

    case kCSSEncodedEntitiesForUnencodables:
      snprintf(replacement, sizeof(UnencodableReplacementArray), "\\%x ",
               code_point);
      return static_cast<uint32_t>(strlen(replacement));

    case kNoUnencodables:
      break;
  }
  NOTREACHED();
  replacement[0] = 0;
  return 0;
}

}  // namespace WTF
