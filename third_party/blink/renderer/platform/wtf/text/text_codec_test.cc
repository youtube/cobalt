/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

namespace {

TEST(TextCodec, HTMLEntityEncoding) {
  UnencodableReplacementArray replacement;
  int size = TextCodec::GetUnencodableReplacement(
      0xE003, kEntitiesForUnencodables, replacement);
  EXPECT_EQ(size, 8);
  EXPECT_EQ(std::string(replacement), "&#57347;");
  EXPECT_EQ(replacement[8], 0);
}

TEST(TextCodec, URLEntityEncoding) {
  UnencodableReplacementArray replacement;
  int size = TextCodec::GetUnencodableReplacement(
      0xE003, kURLEncodedEntitiesForUnencodables, replacement);
  EXPECT_EQ(size, 14);
  EXPECT_EQ(std::string(replacement), "%26%2357347%3B");
  EXPECT_EQ(replacement[14], 0);
}

TEST(TextCodec, CSSEntityEncoding) {
  UnencodableReplacementArray replacement;
  int size = TextCodec::GetUnencodableReplacement(
      0xE003, kCSSEncodedEntitiesForUnencodables, replacement);
  EXPECT_EQ(size, 6);
  EXPECT_EQ(std::string(replacement), "\\e003 ");
  EXPECT_EQ(replacement[6], 0);
}

}  // anonymous namespace
}  // namespace WTF
