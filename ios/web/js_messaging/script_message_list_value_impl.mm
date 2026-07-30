// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/check.h"
#import "base/check_op.h"
#import "ios/web/js_messaging/script_message_value_util.h"
#import "ios/web/public/js_messaging/script_message_list_value.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace web {

ScriptMessageListIterator::ScriptMessageListIterator(PassKey,
                                                     NSArray* data,
                                                     size_t index)
    : data_(data), index_(index) {
  CHECK_LE(index, [data count]);
}

ScriptMessageValue ScriptMessageListIterator::operator*() const {
  CHECK_LT(index_, [data_ count]);
  return CreateScriptMessageValue([data_ objectAtIndex:index_]);
}

ScriptMessageListIterator& ScriptMessageListIterator::operator++() {
  index_++;
  return *this;
}

ScriptMessageListIterator ScriptMessageListIterator::operator++(int) {
  ScriptMessageListIterator copy = *this;
  ++(*this);
  return copy;
}

bool operator==(const ScriptMessageListIterator& a,
                const ScriptMessageListIterator& b) {
  CHECK_EQ(a.data_, b.data_);
  return a.index_ == b.index_;
}

bool operator!=(const ScriptMessageListIterator& a,
                const ScriptMessageListIterator& b) {
  CHECK_EQ(a.data_, b.data_);
  return a.index_ != b.index_;
}

ScriptMessageListValue::ScriptMessageListValue(ScriptMessageListValue&&) =
    default;
ScriptMessageListValue& ScriptMessageListValue::operator=(
    ScriptMessageListValue&&) = default;

ScriptMessageListValue::ScriptMessageListValue(NSArray* value) : data_(value) {}

ScriptMessageListValue::~ScriptMessageListValue() = default;

bool ScriptMessageListValue::empty() const {
  return [data_ count] == 0;
}

size_t ScriptMessageListValue::size() const {
  return [data_ count];
}

std::optional<ScriptMessageValue> ScriptMessageListValue::front() const {
  if (id value = [data_ firstObject]) {
    return CreateScriptMessageValue(value);
  }
  return std::nullopt;
}

std::optional<ScriptMessageValue> ScriptMessageListValue::back() const {
  if (id value = [data_ lastObject]) {
    return CreateScriptMessageValue(value);
  }
  return std::nullopt;
}

ScriptMessageListIterator ScriptMessageListValue::begin() {
  using PassKey = ScriptMessageListIterator::PassKey;
  return ScriptMessageListIterator(PassKey{}, data_, 0);
}

ScriptMessageListIterator ScriptMessageListValue::end() {
  using PassKey = ScriptMessageListIterator::PassKey;
  return ScriptMessageListIterator(PassKey{}, data_, [data_ count]);
}

}  // namespace web
