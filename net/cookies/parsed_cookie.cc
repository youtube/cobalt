// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/parsed_cookie.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace net {

ParsedCookie::ParsedCookie(const std::string& cookie_line)
    : is_valid_(false),
      path_index_(0),
      domain_index_(0),
      mac_key_index_(0),
      mac_algorithm_index_(0),
      expires_index_(0),
      maxage_index_(0),
      secure_index_(0),
      httponly_index_(0) {

  if (cookie_line.size() > kMaxCookieSize) {
    VLOG(1) << "Not parsing cookie, too large: " << cookie_line.size();
    return;
  }

  ParseTokenValuePairs(cookie_line);
  if (!pairs_.empty()) {
    is_valid_ = true;
    SetupAttributes();
  }
}

ParsedCookie::~ParsedCookie() {
}

// Returns true if |c| occurs in |chars|
// TODO(erikwright): maybe make this take an iterator, could check for end also?
static inline bool CharIsA(const char c, const char* chars) {
  return strchr(chars, c) != NULL;
}
// Seek the iterator to the first occurrence of a character in |chars|.
// Returns true if it hit the end, false otherwise.
static inline bool SeekTo(std::string::const_iterator* it,
                          const std::string::const_iterator& end,
                          const char* chars) {
  for (; *it != end && !CharIsA(**it, chars); ++(*it)) {}
  return *it == end;
}
// Seek the iterator to the first occurrence of a character not in |chars|.
// Returns true if it hit the end, false otherwise.
static inline bool SeekPast(std::string::const_iterator* it,
                            const std::string::const_iterator& end,
                            const char* chars) {
  for (; *it != end && CharIsA(**it, chars); ++(*it)) {}
  return *it == end;
}
static inline bool SeekBackPast(std::string::const_iterator* it,
                                const std::string::const_iterator& end,
                                const char* chars) {
  for (; *it != end && CharIsA(**it, chars); --(*it)) {}
  return *it == end;
}

const char ParsedCookie::kTerminator[] = "\n\r\0";
const int ParsedCookie::kTerminatorLen = sizeof(kTerminator) - 1;
const char ParsedCookie::kWhitespace[] = " \t";
const char ParsedCookie::kValueSeparator[] = ";";
const char ParsedCookie::kTokenSeparator[] = ";=";

// Create a cookie-line for the cookie.  For debugging only!
// If we want to use this for something more than debugging, we
// should rewrite it better...
std::string ParsedCookie::DebugString() const {
  std::string out;
  for (PairList::const_iterator it = pairs_.begin();
       it != pairs_.end(); ++it) {
    out.append(it->first);
    out.append("=");
    out.append(it->second);
    out.append("; ");
  }
  return out;
}

std::string::const_iterator ParsedCookie::FindFirstTerminator(
    const std::string& s) {
  std::string::const_iterator end = s.end();
  size_t term_pos =
      s.find_first_of(std::string(kTerminator, kTerminatorLen));
  if (term_pos != std::string::npos) {
    // We found a character we should treat as an end of string.
    end = s.begin() + term_pos;
  }
  return end;
}

bool ParsedCookie::ParseToken(std::string::const_iterator* it,
                              const std::string::const_iterator& end,
                              std::string::const_iterator* token_start,
                              std::string::const_iterator* token_end) {
  DCHECK(it && token_start && token_end);
  std::string::const_iterator token_real_end;

  // Seek past any whitespace before the "token" (the name).
  // token_start should point at the first character in the token
  if (SeekPast(it, end, kWhitespace))
    return false;  // No token, whitespace or empty.
  *token_start = *it;

  // Seek over the token, to the token separator.
  // token_real_end should point at the token separator, i.e. '='.
  // If it == end after the seek, we probably have a token-value.
  SeekTo(it, end, kTokenSeparator);
  token_real_end = *it;

  // Ignore any whitespace between the token and the token separator.
  // token_end should point after the last interesting token character,
  // pointing at either whitespace, or at '=' (and equal to token_real_end).
  if (*it != *token_start) {  // We could have an empty token name.
    --(*it);  // Go back before the token separator.
    // Skip over any whitespace to the first non-whitespace character.
    SeekBackPast(it, *token_start, kWhitespace);
    // Point after it.
    ++(*it);
  }
  *token_end = *it;

  // Seek us back to the end of the token.
  *it = token_real_end;
  return true;
}

void ParsedCookie::ParseValue(std::string::const_iterator* it,
                              const std::string::const_iterator& end,
                              std::string::const_iterator* value_start,
                              std::string::const_iterator* value_end) {
  DCHECK(it && value_start && value_end);

  // Seek past any whitespace that might in-between the token and value.
  SeekPast(it, end, kWhitespace);
  // value_start should point at the first character of the value.
  *value_start = *it;

  // Just look for ';' to terminate ('=' allowed).
  // We can hit the end, maybe they didn't terminate.
  SeekTo(it, end, kValueSeparator);

  // Will be pointed at the ; seperator or the end.
  *value_end = *it;

  // Ignore any unwanted whitespace after the value.
  if (*value_end != *value_start) {  // Could have an empty value
    --(*value_end);
    SeekBackPast(value_end, *value_start, kWhitespace);
    ++(*value_end);
  }
}

std::string ParsedCookie::ParseTokenString(const std::string& token) {
  std::string::const_iterator it = token.begin();
  std::string::const_iterator end = FindFirstTerminator(token);

  std::string::const_iterator token_start, token_end;
  if (ParseToken(&it, end, &token_start, &token_end))
    return std::string(token_start, token_end);
  return std::string();
}

std::string ParsedCookie::ParseValueString(const std::string& value) {
  std::string::const_iterator it = value.begin();
  std::string::const_iterator end = FindFirstTerminator(value);

  std::string::const_iterator value_start, value_end;
  ParseValue(&it, end, &value_start, &value_end);
  return std::string(value_start, value_end);
}

// Parse all token/value pairs and populate pairs_.
void ParsedCookie::ParseTokenValuePairs(const std::string& cookie_line) {
  pairs_.clear();

  // Ok, here we go.  We should be expecting to be starting somewhere
  // before the cookie line, not including any header name...
  std::string::const_iterator start = cookie_line.begin();
  std::string::const_iterator it = start;

  // TODO(erikwright): Make sure we're stripping \r\n in the network code.
  // Then we can log any unexpected terminators.
  std::string::const_iterator end = FindFirstTerminator(cookie_line);

  for (int pair_num = 0; pair_num < kMaxPairs && it != end; ++pair_num) {
    TokenValuePair pair;

    std::string::const_iterator token_start, token_end;
    if (!ParseToken(&it, end, &token_start, &token_end))
      break;

    if (it == end || *it != '=') {
      // We have a token-value, we didn't have any token name.
      if (pair_num == 0) {
        // For the first time around, we want to treat single values
        // as a value with an empty name. (Mozilla bug 169091).
        // IE seems to also have this behavior, ex "AAA", and "AAA=10" will
        // set 2 different cookies, and setting "BBB" will then replace "AAA".
        pair.first = "";
        // Rewind to the beginning of what we thought was the token name,
        // and let it get parsed as a value.
        it = token_start;
      } else {
        // Any not-first attribute we want to treat a value as a
        // name with an empty value...  This is so something like
        // "secure;" will get parsed as a Token name, and not a value.
        pair.first = std::string(token_start, token_end);
      }
    } else {
      // We have a TOKEN=VALUE.
      pair.first = std::string(token_start, token_end);
      ++it;  // Skip past the '='.
    }

    // OK, now try to parse a value.
    std::string::const_iterator value_start, value_end;
    ParseValue(&it, end, &value_start, &value_end);
    // OK, we're finished with a Token/Value.
    pair.second = std::string(value_start, value_end);

    // From RFC2109: "Attributes (names) (attr) are case-insensitive."
    if (pair_num != 0)
      StringToLowerASCII(&pair.first);
    pairs_.push_back(pair);

    // We've processed a token/value pair, we're either at the end of
    // the string or a ValueSeparator like ';', which we want to skip.
    if (it != end)
      ++it;
  }
}

void ParsedCookie::SetupAttributes() {
  static const char kPathTokenName[] = "path";
  static const char kDomainTokenName[] = "domain";
  static const char kMACKeyTokenName[] = "mac-key";
  static const char kMACAlgorithmTokenName[] = "mac-algorithm";
  static const char kExpiresTokenName[] = "expires";
  static const char kMaxAgeTokenName[] = "max-age";
  static const char kSecureTokenName[] = "secure";
  static const char kHttpOnlyTokenName[] = "httponly";

  // We skip over the first token/value, the user supplied one.
  for (size_t i = 1; i < pairs_.size(); ++i) {
    if (pairs_[i].first == kPathTokenName) {
      path_index_ = i;
    } else if (pairs_[i].first == kDomainTokenName) {
      domain_index_ = i;
    } else if (pairs_[i].first == kMACKeyTokenName) {
      mac_key_index_ = i;
    } else if (pairs_[i].first == kMACAlgorithmTokenName) {
      mac_algorithm_index_ = i;
    } else if (pairs_[i].first == kExpiresTokenName) {
      expires_index_ = i;
    } else if (pairs_[i].first == kMaxAgeTokenName) {
      maxage_index_ = i;
    } else if (pairs_[i].first == kSecureTokenName) {
      secure_index_ = i;
    } else if (pairs_[i].first == kHttpOnlyTokenName) {
      httponly_index_ = i;
    } else {
      /* some attribute we don't know or don't care about. */
    }
  }
}

}  // namespace
