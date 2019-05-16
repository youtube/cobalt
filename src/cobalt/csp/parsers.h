// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_CSP_PARSERS_H_
#define COBALT_CSP_PARSERS_H_

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "cobalt/csp/crypto.h"

// Utilities for parsing CSP directive headers. These emulate similar
// string helpers from Blink source.

namespace cobalt {
namespace csp {

typedef std::pair<uint32, DigestValue> HashValue;

enum ReferrerPolicy {
  // https://w3c.github.io/webappsec/specs/referrer-policy/#referrer-policy-state-unsafe-url
  kReferrerPolicyUnsafeUrl,
  // The default policy, if no policy is explicitly set by the page.
  kReferrerPolicyDefault,
  // https://w3c.github.io/webappsec/specs/referrer-policy/#referrer-policy-state-no-referrer-when-downgrade
  kReferrerPolicyNoReferrerWhenDowngrade,
  // https://w3c.github.io/webappsec/specs/referrer-policy/#referrer-policy-state-no-referrer
  kReferrerPolicyNoReferrer,
  // https://w3c.github.io/webappsec/specs/referrer-policy/#referrer-policy-state-origin
  kReferrerPolicyOrigin,
  // https://w3c.github.io/webappsec/specs/referrer-policy/#referrer-policy-state-origin-when-cross-origin
  kReferrerPolicyOriginWhenCrossOrigin,
};

enum ReflectedXSSDisposition {
  kReflectedXSSUnset = 0,
  kAllowReflectedXSS,
  kReflectedXSSInvalid,
  kFilterReflectedXSS,
  kBlockReflectedXSS
};

enum HeaderType { kHeaderTypeReport, kHeaderTypeEnforce };

enum HeaderSource {
  kHeaderSourceHTTP,
  kHeaderSourceMeta,
  kHeaderSourceMetaOutsideHead
};

inline bool SkipExactly(const char** position_ptr, const char* end,
                        char delimiter) {
  const char*& position = *position_ptr;
  if (position < end && *position == delimiter) {
    ++position;
    return true;
  }
  return false;
}

template <bool CharacterPredicate(char)>
inline bool SkipExactly(const char** position_ptr, const char* end) {
  const char*& position = *position_ptr;
  if (position < end && CharacterPredicate(*position)) {
    ++position;
    return true;
  }
  return false;
}

inline bool SkipToken(const char** position_ptr, const char* end,
                      const char* token) {
  const char*& position = *position_ptr;
  const char* current = position;
  while (current < end && *token) {
    if (*current != *token) {
      return false;
    }
    ++current;
    ++token;
  }
  if (*token) {
    return false;
  }

  position = current;
  return true;
}

inline void SkipUntil(const char** position_ptr, const char* end,
                      char delimiter) {
  const char*& position = *position_ptr;
  while (position < end && *position != delimiter) {
    ++position;
  }
}

template <bool CharacterPredicate(char)>
inline void SkipUntil(const char** position_ptr, const char* end) {
  const char*& position = *position_ptr;
  while (position < end && !CharacterPredicate(*position)) {
    ++position;
  }
}

template <bool CharacterPredicate(char)>
inline void SkipWhile(const char** position_ptr, const char* end) {
  const char*& position = *position_ptr;
  while (position < end && CharacterPredicate(*position)) {
    ++position;
  }
}

template <bool CharacterPredicate(char)>
inline void ReverseSkipWhile(const char** position_ptr, const char* start) {
  const char*& position = *position_ptr;
  while (position >= start && CharacterPredicate(*position)) {
    --position;
  }
}

inline bool IsNotAsciiWhitespace(char c) { return !base::IsAsciiWhitespace(c); }

inline bool IsAsciiAlphanumeric(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c);
}

inline bool IsCSPDirectiveNameCharacter(char c) {
  return IsAsciiAlphanumeric(c) || c == '-';
}

inline bool IsCSPDirectiveValueCharacter(char c) {
  return base::IsAsciiWhitespace(c) ||
         (c >= 0x21 && c <= 0x7e);  // Whitespace + VCHAR
}

// Only checks for general Base64(url) encoded chars, not '=' chars since '=' is
// positional and may only appear at the end of a Base64 encoded string.
inline bool IsBase64EncodedCharacter(char c) {
  return IsAsciiAlphanumeric(c) || c == '+' || c == '/' || c == '-' || c == '_';
}

inline bool IsNonceCharacter(char c) {
  return IsBase64EncodedCharacter(c) || c == '=';
}

inline bool IsSourceCharacter(char c) { return IsNotAsciiWhitespace(c); }

inline bool IsPathComponentCharacter(char c) { return c != '?' && c != '#'; }

inline bool IsHostCharacter(char c) {
  return IsAsciiAlphanumeric(c) || c == '-';
}

inline bool IsSchemeContinuationCharacter(char c) {
  return IsAsciiAlphanumeric(c) || c == '+' || c == '-' || c == '.';
}

inline bool IsNotColonOrSlash(char c) { return c != ':' && c != '/'; }

inline bool IsMediaTypeCharacter(char c) {
  return IsNotAsciiWhitespace(c) && c != '/';
}

static inline std::string ToString(const char* begin, const char* end) {
  return std::string(begin, static_cast<size_t>(end - begin));
}

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_PARSERS_H_
