// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_PATTERN_ACCOUNT_RESTRICTION_H_
#define IOS_CHROME_BROWSER_SIGNIN_PATTERN_ACCOUNT_RESTRICTION_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

// This code is adapted from
// //components/signin/public/android/java/src/org/
// chromium/components/signin/PatternMatcher.java

// Contains a list of chunks.
class Pattern {
 public:
  explicit Pattern(std::vector<std::string> chunks);
  ~Pattern();
  // Copyable
  Pattern(const Pattern&);
  Pattern& operator=(const Pattern&);
  // Moveable
  Pattern(Pattern&& from);
  Pattern& operator=(Pattern&& from);

  // Checks whether the whole given string matches the chunks pattern.
  bool Match(base::StringPiece string) const;

 private:
  std::vector<std::string> chunks_;
};

// Contains a list of patterns.
class PatternAccountRestriction {
 public:
  PatternAccountRestriction();
  explicit PatternAccountRestriction(std::vector<Pattern> patterns);
  ~PatternAccountRestriction();
  // Non-copyable
  PatternAccountRestriction(const PatternAccountRestriction& other) = delete;
  PatternAccountRestriction& operator=(const PatternAccountRestriction& other) =
      delete;
  // Moveable
  PatternAccountRestriction(PatternAccountRestriction&& from);
  PatternAccountRestriction& operator=(PatternAccountRestriction&& from);

  // Checks if the account is restricted according to the given email.
  bool IsAccountRestricted(base::StringPiece email) const;

 private:
  std::vector<Pattern> patterns_;
};

// Returns true if `value` holds a correct list of patterns. If one of the
// pattern is invalid, returns false.
bool ArePatternsValid(const base::Value* value);

// Creates a PatternAccountRestriction from `list` which needs to
// be a list of strings.
absl::optional<PatternAccountRestriction> PatternAccountRestrictionFromValue(
    const base::Value::List& list);

// The given chunk is split by wildcards and a Pattern (list of chunks) is
// returned. The first chunk contains pattern characters from the beginning to
// the first wildcard, the second chunk contains characters from the first
// wildcard to the second one, etc.
absl::optional<Pattern> PatternFromString(base::StringPiece chunk);

#endif  // IOS_CHROME_BROWSER_SIGNIN_PATTERN_ACCOUNT_RESTRICTION_H_
