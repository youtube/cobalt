/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"

#include "base/strings/escape.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace blink {

void ReplaceNBSPWithSpace(String& str) {
  static const UChar kNonBreakingSpaceCharacter = 0xA0;
  static const UChar kSpaceCharacter = ' ';
  str.Replace(kNonBreakingSpaceCharacter, kSpaceCharacter);
}

String ConvertURIListToURL(const String& uri_list) {
  Vector<String> items;
  // Line separator is \r\n per RFC 2483 - however, for compatibility
  // reasons we allow just \n here.
  uri_list.Split('\n', items);
  // Process the input and return the first valid URL. In case no URLs can
  // be found, return an empty string. This is in line with the HTML5 spec.
  for (String& line : items) {
    line = line.StripWhiteSpace();
    if (line.empty())
      continue;
    if (line[0] == '#')
      continue;
    KURL url = KURL(line);
    if (url.IsValid())
      return url;
  }
  return String();
}

static String EscapeForHTML(const String& str) {
  // base::EscapeForHTML can work on 8-bit Latin-1 strings as well as 16-bit
  // strings.
  if (str.Is8Bit()) {
    auto result = base::EscapeForHTML(
        {reinterpret_cast<const char*>(str.Characters8()), str.length()});
    return String(result.data(), result.size());
  }
  auto result = base::EscapeForHTML({str.Characters16(), str.length()});
  return String(result.data(), result.size());
}

String URLToImageMarkup(const KURL& url, const String& title) {
  StringBuilder builder;
  builder.Append("<img src=\"");
  builder.Append(EscapeForHTML(url.GetString()));
  builder.Append("\"");
  if (!title.empty()) {
    builder.Append(" alt=\"");
    builder.Append(EscapeForHTML(title));
    builder.Append("\"");
  }
  builder.Append("/>");
  return builder.ToString();
}

String PNGToImageMarkup(const mojo_base::BigBuffer& png_data) {
  if (!png_data.size())
    return String();

  StringBuilder markup;
  markup.Append("<img src=\"data:image/png;base64,");
  markup.Append(Base64Encode(png_data));
  markup.Append("\" alt=\"\"/>");
  return markup.ToString();
}

}  // namespace blink
