// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SAMPLES_SIMPLE_EXAMPLE_SIMPLE_EXAMPLE_H_
#define COBALT_SAMPLES_SIMPLE_EXAMPLE_SIMPLE_EXAMPLE_H_

namespace cobalt {
namespace samples {

class SimpleExample {
 public:
  explicit SimpleExample(int multiplier);
  virtual ~SimpleExample();

  // Performs a multiple and add operation and returns the result.
  // result = multiplier_ * a + b
  int MultiplyAdd(int a, int b) const;

  void set_multiplier(int multiplier) { multiplier_ = multiplier; }
  int multiplier() const { return multiplier_; }

 private:
  int multiplier_;
};

}  // namespace samples
}  // namespace cobalt

#endif  // COBALT_SAMPLES_SIMPLE_EXAMPLE_SIMPLE_EXAMPLE_H_
