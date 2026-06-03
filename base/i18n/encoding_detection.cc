// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/encoding_detection.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#if !BUILDFLAG(IS_COBALT)
#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"  // nogncheck
#endif  // !BUILDFLAG(IS_COBALT)

// third_party/ced/src/util/encodings/encodings.h, which is included
// by the include above, undefs UNICODE because that is a macro used
// internally in ced. If we later in the same translation unit do
// anything related to Windows or Windows headers those will then use
// the ASCII versions which we do not want. To avoid that happening in
// jumbo builds, we redefine UNICODE again here.
#if BUILDFLAG(IS_WIN)
#define UNICODE 1
#endif  // BUILDFLAG(IS_WIN)

namespace base {

namespace {

// Include 7-bit encodings
constexpr bool kIgnore7bitEncodings = false;

// Plain text
constexpr CompactEncDet::TextCorpusType kPlainTextCorpus =
    CompactEncDet::QUERY_CORPUS;

}  // namespace

bool DetectEncoding(std::string_view text, std::string* encoding) {
#if !BUILDFLAG(IS_COBALT)
  int consumed_bytes;
  bool is_reliable;
  Encoding enc = CompactEncDet::DetectEncoding(
      /*text=*/text.data(), /*text_length=*/text.size(), /*url_hint=*/nullptr,
      /*http_charset_hint=*/nullptr, /*meta_charset_hint=*/nullptr,
      /*encoding_hint=*/UNKNOWN_ENCODING,
      /*language_hint=*/UNKNOWN_LANGUAGE,
      /*corpus_type=*/kPlainTextCorpus,
      /*ignore_7bit_mail_encodings=*/kIgnore7bitEncodings,
      /*bytes_consumed=*/&consumed_bytes,
      /*is_reliable=*/&is_reliable);

  if (enc == UNKNOWN_ENCODING) {
    return false;
  }

  *encoding = MimeEncodingName(enc);
  return true;
#else
  // Cobalt always uses UTF-8 and does not need dynamic encoding detection.
  *encoding = "UTF-8";
  return true;
#endif  // !BUILDFLAG(IS_COBALT)
}

}  // namespace base
