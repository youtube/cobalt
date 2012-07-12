// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_PARSED_COOKIE_H_
#define NET_COOKIES_PARSED_COOKIE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT ParsedCookie {
 public:
  typedef std::pair<std::string, std::string> TokenValuePair;
  typedef std::vector<TokenValuePair> PairList;

  // The maximum length of a cookie string we will try to parse
  static const size_t kMaxCookieSize = 4096;
  // The maximum number of Token/Value pairs.  Shouldn't have more than 8.
  static const int kMaxPairs = 16;

  // Construct from a cookie string like "BLAH=1; path=/; domain=.google.com"
  ParsedCookie(const std::string& cookie_line);
  ~ParsedCookie();

  // You should not call any other methods on the class if !IsValid
  bool IsValid() const { return is_valid_; }

  const std::string& Name() const { return pairs_[0].first; }
  const std::string& Token() const { return Name(); }
  const std::string& Value() const { return pairs_[0].second; }

  bool HasPath() const { return path_index_ != 0; }
  const std::string& Path() const { return pairs_[path_index_].second; }
  bool HasDomain() const { return domain_index_ != 0; }
  const std::string& Domain() const { return pairs_[domain_index_].second; }
  bool HasMACKey() const { return mac_key_index_ != 0; }
  const std::string& MACKey() const { return pairs_[mac_key_index_].second; }
  bool HasMACAlgorithm() const { return mac_algorithm_index_ != 0; }
  const std::string& MACAlgorithm() const {
    return pairs_[mac_algorithm_index_].second;
  }
  bool HasExpires() const { return expires_index_ != 0; }
  const std::string& Expires() const { return pairs_[expires_index_].second; }
  bool HasMaxAge() const { return maxage_index_ != 0; }
  const std::string& MaxAge() const { return pairs_[maxage_index_].second; }
  bool IsSecure() const { return secure_index_ != 0; }
  bool IsHttpOnly() const { return httponly_index_ != 0; }

  // Returns the number of attributes, for example, returning 2 for:
  //   "BLAH=hah; path=/; domain=.google.com"
  size_t NumberOfAttributes() const { return pairs_.size() - 1; }

  // For debugging only!
  std::string DebugString() const;

  // Returns an iterator pointing to the first terminator character found in
  // the given string.
  static std::string::const_iterator FindFirstTerminator(const std::string& s);

  // Given iterators pointing to the beginning and end of a string segment,
  // returns as output arguments token_start and token_end to the start and end
  // positions of a cookie attribute token name parsed from the segment, and
  // updates the segment iterator to point to the next segment to be parsed.
  // If no token is found, the function returns false.
  static bool ParseToken(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         std::string::const_iterator* token_start,
                         std::string::const_iterator* token_end);

  // Given iterators pointing to the beginning and end of a string segment,
  // returns as output arguments value_start and value_end to the start and end
  // positions of a cookie attribute value parsed from the segment, and updates
  // the segment iterator to point to the next segment to be parsed.
  static void ParseValue(std::string::const_iterator* it,
                         const std::string::const_iterator& end,
                         std::string::const_iterator* value_start,
                         std::string::const_iterator* value_end);

  // Same as the above functions, except the input is assumed to contain the
  // desired token/value and nothing else.
  static std::string ParseTokenString(const std::string& token);
  static std::string ParseValueString(const std::string& value);

 private:
  static const char kTerminator[];
  static const int  kTerminatorLen;
  static const char kWhitespace[];
  static const char kValueSeparator[];
  static const char kTokenSeparator[];

  void ParseTokenValuePairs(const std::string& cookie_line);
  void SetupAttributes();

  PairList pairs_;
  bool is_valid_;
  // These will default to 0, but that should never be valid since the
  // 0th index is the user supplied token/value, not an attribute.
  // We're really never going to have more than like 8 attributes, so we
  // could fit these into 3 bits each if we're worried about size...
  size_t path_index_;
  size_t domain_index_;
  size_t mac_key_index_;
  size_t mac_algorithm_index_;
  size_t expires_index_;
  size_t maxage_index_;
  size_t secure_index_;
  size_t httponly_index_;

  DISALLOW_COPY_AND_ASSIGN(ParsedCookie);
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_H_
