// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_utils/dates.h"

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chrome_pdf {

namespace {

class DateDeserializer final {
 public:
  // `parsing` must outlive `this` because `base::StringPiece` has reference
  // semantics.
  explicit DateDeserializer(base::StringPiece parsing)
      : deserializing_(parsing) {}
  ~DateDeserializer() = default;

  // Pops the first `num_digits` characters from the string and converts them to
  // an int if possible. Popping too many characters or characters that cannot
  // be converted puts the deserializer in a stopped state.
  absl::optional<int> PopDigits(size_t num_digits) {
    if (stopped_)
      return absl::nullopt;

    // `base::StringToUint()` allows leading sign characters, so also verify
    // that the front character is a digit.
    uint32_t value;
    if (deserializing_.size() < num_digits ||
        !base::IsAsciiDigit(deserializing_.front()) ||
        !base::StringToUint(deserializing_.substr(0, num_digits), &value)) {
      stopped_ = true;
      return absl::nullopt;
    }

    // Pop front characters.
    deserializing_ = deserializing_.substr(num_digits);
    return value;
  }

  // Pops the front character if it is not a digit. Otherwise, does not change
  // the state of the deserializer and returns `absl::nullopt`.
  absl::optional<char> TryPopNonDigit() {
    if (stopped_ || deserializing_.empty())
      return absl::nullopt;

    const char front = deserializing_.front();
    if (base::IsAsciiDigit(front))
      return absl::nullopt;

    deserializing_ = deserializing_.substr(1);
    return front;
  }

  // Takes the deserializer out of a stopped state.
  void unstop() { stopped_ = false; }

 private:
  base::StringPiece deserializing_;
  bool stopped_ = false;
};

// Parses the offset info in `deserializer`, which is the time offset portion of
// the date format provided in section 7.9.4 "Dates" of the ISO 32000-1:2008
// spec. An input is expected to look like "HH'mm", such that "HH" is the hour
// and "mm" is the minute.
base::TimeDelta ParseOffset(DateDeserializer& deserializer) {
  base::TimeDelta offset;

  // UTC is assumed if no time zone information is provided.
  const absl::optional<char> sign = deserializer.TryPopNonDigit();
  if (!sign.has_value() || (sign.value() != '+' && sign.value() != '-'))
    return offset;

  offset += base::Hours(deserializer.PopDigits(2).value_or(0));

  // The spec requires that the hours offset be followed by an apostrophe, but
  // don't be strict about its presence.
  const absl::optional<char> apostrophe = deserializer.TryPopNonDigit();
  if (apostrophe.has_value() && apostrophe.value() != '\'')
    return sign.value() == '+' ? offset : -offset;

  // The minutes offset follows the hours offset. Be lenient about anything
  // following the minutes offset. One reason for the leniency is the apostrophe
  // following the minues, which is only mentioned in earlier versions of the
  // spec.
  offset += base::Minutes(deserializer.PopDigits(2).value_or(0));

  return sign.value() == '+' ? offset : -offset;
}

}  // namespace

base::Time ParsePdfDate(base::StringPiece date) {
  // The prefix "D:" is required according to the spec, but don't require it as
  // earlier versions of the spec weren't strict about it.
  if (date.substr(0, 2) == "D:")
    date = date.substr(2);

  DateDeserializer deserializer(date);

  // Year is the only required part of a valid date.
  const absl::optional<int> deserialized_year = deserializer.PopDigits(4);
  if (!deserialized_year.has_value())
    return base::Time();

  // Month and day default to 1. The rest of the parts of a date default to 0.
  base::Time::Exploded exploded = {};
  exploded.year = deserialized_year.value();
  exploded.month = deserializer.PopDigits(2).value_or(1);
  exploded.day_of_month = deserializer.PopDigits(2).value_or(1);
  exploded.hour = deserializer.PopDigits(2).value_or(0);
  exploded.minute = deserializer.PopDigits(2).value_or(0);
  exploded.second = deserializer.PopDigits(2).value_or(0);

  base::Time parsed;
  if (!base::Time::FromUTCExploded(exploded, &parsed))
    return base::Time();

  // `base::Time` is in UTC, so `parsed` must be normalized if there is an
  // offset.
  deserializer.unstop();
  return parsed - ParseOffset(deserializer);
}

}  // namespace chrome_pdf
