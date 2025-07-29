/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_SHARED_LIB_TEST_UTILS_H_
#define SRC_SHARED_LIB_TEST_UTILS_H_

#include <cassert>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "perfetto/public/abi/pb_decoder_abi.h"
#include "perfetto/public/pb_utils.h"
#include "perfetto/public/tracing_session.h"

#include "test/gtest_and_gmock.h"

// Pretty printer for gtest
void PrintTo(const PerfettoPbDecoderField& field, std::ostream*);

namespace perfetto {
namespace shlib {
namespace test_utils {

class TracingSession {
 public:
  class Builder {
   public:
    Builder() = default;
    Builder& set_data_source_name(std::string data_source_name) {
      data_source_name_ = std::move(data_source_name);
      return *this;
    }
    TracingSession Build();

   private:
    std::string data_source_name_;
  };
  TracingSession(TracingSession&&) noexcept;

  ~TracingSession();

  struct PerfettoTracingSessionImpl* session() const { return session_; }

  void StopBlocking();
  std::vector<uint8_t> ReadBlocking();

 private:
  TracingSession() = default;
  struct PerfettoTracingSessionImpl* session_;
  bool stopped_ = false;
};

template <typename FieldSkipper>
class FieldViewBase {
 public:
  class Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = const PerfettoPbDecoderField;
    using pointer = value_type;
    using reference = value_type;
    reference operator*() const {
      struct PerfettoPbDecoder decoder;
      decoder.read_ptr = read_ptr_;
      decoder.end_ptr = end_ptr_;
      struct PerfettoPbDecoderField field;
      do {
        field = PerfettoPbDecoderParseField(&decoder);
      } while (field.status == PERFETTO_PB_DECODER_OK &&
               skipper_.ShouldSkip(field));
      return field;
    }
    Iterator& operator++() {
      struct PerfettoPbDecoder decoder;
      decoder.read_ptr = read_ptr_;
      decoder.end_ptr = end_ptr_;
      PerfettoPbDecoderSkipField(&decoder);
      read_ptr_ = decoder.read_ptr;
      AdvanceToFirstInterestingField();
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.read_ptr_ == b.read_ptr_;
    }
    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return a.read_ptr_ != b.read_ptr_;
    }

   private:
    Iterator(const uint8_t* read_ptr,
             const uint8_t* end_ptr,
             const FieldSkipper& skipper)
        : read_ptr_(read_ptr), end_ptr_(end_ptr), skipper_(skipper) {
      AdvanceToFirstInterestingField();
    }
    void AdvanceToFirstInterestingField() {
      struct PerfettoPbDecoder decoder;
      decoder.read_ptr = read_ptr_;
      decoder.end_ptr = end_ptr_;
      struct PerfettoPbDecoderField field;
      const uint8_t* prev_read_ptr;
      do {
        prev_read_ptr = decoder.read_ptr;
        field = PerfettoPbDecoderParseField(&decoder);
      } while (field.status == PERFETTO_PB_DECODER_OK &&
               skipper_.ShouldSkip(field));
      if (field.status == PERFETTO_PB_DECODER_OK) {
        read_ptr_ = prev_read_ptr;
      } else {
        read_ptr_ = decoder.read_ptr;
      }
    }
    friend class FieldViewBase<FieldSkipper>;
    const uint8_t* read_ptr_;
    const uint8_t* end_ptr_;
    const FieldSkipper& skipper_;
  };
  using value_type = const PerfettoPbDecoderField;
  using const_iterator = Iterator;
  template <typename... Args>
  explicit FieldViewBase(const uint8_t* begin, const uint8_t* end, Args... args)
      : begin_(begin), end_(end), s_(args...) {}
  template <typename... Args>
  explicit FieldViewBase(const std::vector<uint8_t>& data, Args... args)
      : FieldViewBase(data.data(), data.data() + data.size(), args...) {}
  template <typename... Args>
  explicit FieldViewBase(const struct PerfettoPbDecoderField& field,
                         Args... args)
      : s_(args...) {
    if (field.wire_type != PERFETTO_PB_WIRE_TYPE_DELIMITED) {
      abort();
    }
    begin_ = field.value.delimited.start;
    end_ = begin_ + field.value.delimited.len;
  }
  Iterator begin() const { return Iterator(begin_, end_, s_); }
  Iterator end() const { return Iterator(end_, end_, s_); }
  PerfettoPbDecoderField front() const { return *begin(); }

  size_t size() const {
    size_t count = 0;
    for (auto field : *this) {
      (void)field;
      count++;
    }
    return count;
  }

  bool ok() const {
    for (auto field : *this) {
      if (field.status != PERFETTO_PB_DECODER_OK) {
        return false;
      }
    }
    return true;
  }

 private:
  const uint8_t* begin_;
  const uint8_t* end_;
  FieldSkipper s_;
};

// Pretty printer for gtest
template <typename FieldSkipper>
void PrintTo(const FieldViewBase<FieldSkipper>& field_view, std::ostream* pos) {
  std::ostream& os = *pos;
  os << "{";
  for (PerfettoPbDecoderField f : field_view) {
    PrintTo(f, pos);
    os << ", ";
  }
  os << "}";
}

class IdFieldSkipper {
 public:
  explicit IdFieldSkipper(uint32_t id) : id_(id) {}
  explicit IdFieldSkipper(int32_t id) : id_(static_cast<uint32_t>(id)) {}
  bool ShouldSkip(const struct PerfettoPbDecoderField& field) const {
    return field.id != id_;
  }

 private:
  uint32_t id_;
};

class NoFieldSkipper {
 public:
  NoFieldSkipper() = default;
  bool ShouldSkip(const struct PerfettoPbDecoderField&) const { return false; }
};

// View over all the fields of a contiguous serialized protobuf message.
//
// Examples:
//
// for (struct PerfettoPbDecoderField field : FieldView(msg_begin, msg_end)) {
//   //...
// }
// FieldView fields2(/*PerfettoPbDecoderField*/ nested_field);
// FieldView fields3(/*std::vector<uint8_t>*/ data);
// size_t num = fields1.size(); // The number of fields.
// bool ok = fields1.ok(); // Checks that the message is not malformed.
using FieldView = FieldViewBase<NoFieldSkipper>;

// Like `FieldView`, but only considers fields with a specific id.
//
// Examples:
//
// IdFieldView fields(msg_begin, msg_end, id)
using IdFieldView = FieldViewBase<IdFieldSkipper>;

// Matches a PerfettoPbDecoderField with the specified id. Accepts another
// matcher to match the contents of the field.
//
// Example:
// PerfettoPbDecoderField field = ...
// EXPECT_THAT(field, PbField(900, VarIntField(5)));
template <typename M>
auto PbField(int32_t id, M m) {
  return testing::AllOf(
      testing::Field(&PerfettoPbDecoderField::status, PERFETTO_PB_DECODER_OK),
      testing::Field(&PerfettoPbDecoderField::id, id), m);
}

// Matches a PerfettoPbDecoderField submessage field. Accepts a container
// matcher for the subfields.
//
// Example:
// PerfettoPbDecoderField field = ...
// EXPECT_THAT(field, MsgField(ElementsAre(...)));
template <typename M>
auto MsgField(M m) {
  auto f = [](const PerfettoPbDecoderField& field) { return FieldView(field); };
  return testing::AllOf(
      testing::Field(&PerfettoPbDecoderField::status, PERFETTO_PB_DECODER_OK),
      testing::Field(&PerfettoPbDecoderField::wire_type,
                     PERFETTO_PB_WIRE_TYPE_DELIMITED),
      testing::ResultOf(f, m));
}

// Matches a PerfettoPbDecoderField length delimited field. Accepts a string
// matcher.
//
// Example:
// PerfettoPbDecoderField field = ...
// EXPECT_THAT(field, StringField("string"));
template <typename M>
auto StringField(M m) {
  auto f = [](const PerfettoPbDecoderField& field) {
    return std::string(
        reinterpret_cast<const char*>(field.value.delimited.start),
        field.value.delimited.len);
  };
  return testing::AllOf(
      testing::Field(&PerfettoPbDecoderField::status, PERFETTO_PB_DECODER_OK),
      testing::Field(&PerfettoPbDecoderField::wire_type,
                     PERFETTO_PB_WIRE_TYPE_DELIMITED),
      testing::ResultOf(f, m));
}

// Matches a PerfettoPbDecoderField VarInt field. Accepts an integer matcher
//
// Example:
// PerfettoPbDecoderField field = ...
// EXPECT_THAT(field, VarIntField(1)));
template <typename M>
auto VarIntField(M m) {
  auto f = [](const PerfettoPbDecoderField& field) {
    return field.value.integer64;
  };
  return testing::AllOf(
      testing::Field(&PerfettoPbDecoderField::status, PERFETTO_PB_DECODER_OK),
      testing::Field(&PerfettoPbDecoderField::wire_type,
                     PERFETTO_PB_WIRE_TYPE_VARINT),
      testing::ResultOf(f, m));
}

}  // namespace test_utils
}  // namespace shlib
}  // namespace perfetto

#endif  // SRC_SHARED_LIB_TEST_UTILS_H_
