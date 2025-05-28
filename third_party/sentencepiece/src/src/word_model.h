// Copyright 2016 Google Inc.
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
// limitations under the License.!

#ifndef WORD_MODEL_H_
#define WORD_MODEL_H_

#include "model_interface.h"
#include "sentencepiece_model.pb.h"

namespace sentencepiece {
namespace word {

// Tokenize text with whitespaces.
class Model : public ModelInterface {
 public:
  explicit Model(const ModelProto &model_proto);
  ~Model() override;

  EncodeResult Encode(absl::string_view normalized) const override;
};
}  // namespace word
}  // namespace sentencepiece
#endif  // WORD_MODEL_H_
