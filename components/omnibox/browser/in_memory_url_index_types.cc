// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/in_memory_url_index_types.h"

#include <functional>
#include <iterator>
#include <numeric>
#include <set>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"

namespace {

// The maximum number of characters to consider from a URL and page title while
// matching user-typed terms.
const size_t kMaxSignificantChars = 200;

void String16VectorFromString16Internal(std::u16string word,
                                        size_t previous_position,
                                        String16Vector* words,
                                        WordStarts* word_starts) {
  size_t initial_whitespace = 0;
  if (word.empty())
    return;
  words->push_back(word);
  if (!word_starts)
    return;
  size_t word_start = previous_position + initial_whitespace;
  if (word_start < kMaxSignificantChars)
    word_starts->push_back(word_start);
}

}  // namespace

// Matches within URL and Title Strings ----------------------------------------

TermMatches MatchTermsInString(const String16Vector& terms,
                               const std::u16string& cleaned_string) {
  TermMatches matches;
  for (size_t i = 0; i < terms.size(); ++i) {
    TermMatches term_matches = MatchTermInString(terms[i], cleaned_string, i);
    matches.insert(matches.end(), term_matches.begin(), term_matches.end());
  }
  return matches;
}

TermMatches MatchTermInString(const std::u16string& term,
                              const std::u16string& cleaned_string,
                              int term_num) {
  const size_t kMaxCompareLength = 2048;
  const std::u16string& short_string =
      (cleaned_string.length() > kMaxCompareLength)
          ? cleaned_string.substr(0, kMaxCompareLength)
          : cleaned_string;
  TermMatches matches;
  for (size_t location = short_string.find(term);
       location != std::u16string::npos;
       location = short_string.find(term, location + 1))
    matches.push_back(TermMatch(term_num, location, term.length()));
  return matches;
}

// Comparison function for sorting TermMatches by their offsets.
bool SortMatchComparator(const TermMatch& m1, const TermMatch& m2) {
  // Return the match that occurs first (smallest offset). In the case of a tie,
  // return the longer match.
  return m1.offset == m2.offset ? m1.length > m2.length : m1.offset < m2.offset;
}

TermMatches SortMatches(const TermMatches& matches) {
  TermMatches sorted_matches(matches);
  std::sort(sorted_matches.begin(), sorted_matches.end(), SortMatchComparator);
  return sorted_matches;
}

// Assumes |sorted_matches| is already sorted.
TermMatches DeoverlapMatches(const TermMatches& sorted_matches) {
  TermMatches out;
  base::ranges::copy_if(
      sorted_matches, std::back_inserter(out), [&out](const TermMatch& match) {
        return out.empty() ||
               match.offset >= (out.back().offset + out.back().length);
      });
  return out;
}

std::vector<size_t> OffsetsFromTermMatches(const TermMatches& matches) {
  std::vector<size_t> offsets;
  for (const auto& match : matches) {
    offsets.push_back(match.offset);
    offsets.push_back(match.offset + match.length);
  }
  return offsets;
}

TermMatches ReplaceOffsetsInTermMatches(const TermMatches& matches,
                                        const std::vector<size_t>& offsets) {
  DCHECK_EQ(2 * matches.size(), offsets.size());
  TermMatches new_matches;
  auto offset_iter = offsets.begin();
  for (auto term_iter = matches.begin(); term_iter != matches.end();
       ++term_iter, ++offset_iter) {
    const size_t starting_offset = *offset_iter;
    ++offset_iter;
    const size_t ending_offset = *offset_iter;
    if ((starting_offset != std::u16string::npos) &&
        (ending_offset != std::u16string::npos) &&
        (starting_offset != ending_offset)) {
      TermMatch new_match(*term_iter);
      new_match.offset = starting_offset;
      new_match.length = ending_offset - starting_offset;
      new_matches.push_back(new_match);
    }
  }
  return new_matches;
}

// Utility Functions -----------------------------------------------------------

String16Set String16SetFromString16(const std::u16string& cleaned_uni_string,
                                    WordStarts* word_starts) {
  String16Vector words =
      String16VectorFromString16(cleaned_uni_string, word_starts);
  for (auto& word : words)
    word = base::i18n::ToLower(word).substr(0, kMaxSignificantChars);
  return String16Set(std::make_move_iterator(words.begin()),
                     std::make_move_iterator(words.end()));
}

String16Vector String16VectorFromString16(
    const std::u16string& cleaned_uni_string,
    WordStarts* word_starts) {
  if (word_starts)
    word_starts->clear();

  String16Vector words;
  TailoredWordBreakIterator iter(cleaned_uni_string,
                                 base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init())
    return words;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      String16VectorFromString16Internal(iter.GetString(), iter.prev(), &words,
                                         word_starts);
    }
  }
  return words;
}

Char16Set Char16SetFromString16(const std::u16string& term) {
  return Char16Set(term.begin(), term.end());
}

// HistoryInfoMapValue ---------------------------------------------------------

HistoryInfoMapValue::HistoryInfoMapValue() = default;
HistoryInfoMapValue::HistoryInfoMapValue(const HistoryInfoMapValue& other) =
    default;
HistoryInfoMapValue::HistoryInfoMapValue(HistoryInfoMapValue&& other) = default;
HistoryInfoMapValue& HistoryInfoMapValue::operator=(
    const HistoryInfoMapValue& other) = default;
HistoryInfoMapValue& HistoryInfoMapValue::operator=(
    HistoryInfoMapValue&& other) = default;
HistoryInfoMapValue::~HistoryInfoMapValue() = default;

size_t HistoryInfoMapValue::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_row) +
         base::trace_event::EstimateMemoryUsage(visits);
}

// RowWordStarts ---------------------------------------------------------------

RowWordStarts::RowWordStarts() = default;
RowWordStarts::RowWordStarts(const RowWordStarts& other) = default;
RowWordStarts::RowWordStarts(RowWordStarts&& other) = default;
RowWordStarts& RowWordStarts::operator=(const RowWordStarts& other) = default;
RowWordStarts& RowWordStarts::operator=(RowWordStarts&& other) = default;
RowWordStarts::~RowWordStarts() = default;

size_t RowWordStarts::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_word_starts_) +
         base::trace_event::EstimateMemoryUsage(title_word_starts_);
}

void RowWordStarts::Clear() {
  url_word_starts_.clear();
  title_word_starts_.clear();
}
