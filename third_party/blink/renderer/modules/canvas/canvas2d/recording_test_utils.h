// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_RECORDING_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_RECORDING_TEST_UTILS_H_

#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_record.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink_testing {

// A view of a `cc::PaintRecord` which drops the leading and trailing
// `cc::SaveOp` and `cc::RestoreOp` that are present in every single Canvas 2D
// recordings.
class RecordedOpsView {
 public:
  explicit RecordedOpsView(cc::PaintRecord record);

  using value_type = cc::PaintOp;
  size_t size() const { return record_.size() - 2; }
  bool empty() const { return size() == 0; }
  cc::PaintOpBuffer::Iterator begin() const { return begin_; }
  cc::PaintOpBuffer::Iterator end() const { return end_; }

 private:
  cc::PaintRecord record_;
  cc::PaintOpBuffer::Iterator begin_;
  cc::PaintOpBuffer::Iterator end_;
};

// A Google Test matcher that checks whether a `cc::PaintRecord` contains the
// expected paint ops, ignoring the leading and trailing `cc::SaveOp` and
// `cc::RestoreOp` that are present in every single Canvas 2D recordings.
template <typename... Args>
testing::Matcher<cc::PaintRecord> RecordedOpsAre(Args... args) {
  return testing::ResultOf(
      [](const cc::PaintRecord& record) { return RecordedOpsView(record); },
      testing::ElementsAre(args...));
}

}  // namespace blink_testing

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_RECORDING_TEST_UTILS_H_
