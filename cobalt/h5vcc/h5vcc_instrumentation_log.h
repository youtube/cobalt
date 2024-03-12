// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_INSTRUMENTATION_LOG_H_
#define COBALT_H5VCC_H5VCC_INSTRUMENTATION_LOG_H_

#include <string>
#include <vector>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class CircularBuffer {
 public:
  explicit CircularBuffer(int capacity)
      : capacity_(capacity), buffer_(capacity) {}

  void Push(const std::string& value) {
    buffer_[tail_] = value;
    tail_ = (tail_ + 1) % capacity_;
  }

  std::string GetLast() {
    if (tail_ == 0) {
      return buffer_[capacity_ - 1];
    }

    return buffer_[tail_ - 1];
  }

  std::vector<std::string> GetValues() {
    std::vector<std::string> values;

    for (int i = tail_; i < capacity_; i++) {
      values.push_back(buffer_[i]);
    }

    for (int i = 0; i < tail_; i++) {
      values.push_back(buffer_[i]);
    }

    return values;
  }

 private:
  int capacity_;
  std::vector<std::string> buffer_;
  int tail_ = 0;
};

class InstrumentationLog {
 public:
  static CircularBuffer* buffer_;

  static void LogEvent(std::string event);
  static script::Sequence<std::string> GetLogTrace();
  static std::string GetLogTraceAsJson();
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_INSTRUMENTATION_LOG_H_
