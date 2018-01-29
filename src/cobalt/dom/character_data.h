// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_CHARACTER_DATA_H_
#define COBALT_DOM_CHARACTER_DATA_H_

#include <string>

#include "base/string_piece.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// Each node inheriting from the CharacterData interface has an associated
// mutable string called data.
//   https://www.w3.org/TR/2015/WD-dom-20150428/#interface-characterdata
class CharacterData : public Node {
 public:
  // Web API: Node
  base::optional<std::string> node_value() const override { return data_; }
  void set_node_value(const base::optional<std::string>& node_value) override {
    // Don't use value_or to avoid copying the string.
    if (node_value) {
      set_data(node_value.value());
    } else {
      set_data("");
    }
  }

  base::optional<std::string> text_content() const override { return data_; }
  void set_text_content(
      const base::optional<std::string>& text_content) override {
    // Don't use value_or to avoid copying the string.
    if (text_content) {
      set_data(text_content.value());
    } else {
      set_data("");
    }
  }

  // Web API: CharacterData
  //
  std::string data() const { return data_; }
  void set_data(const std::string& data);

  // Custom, not in any spec.
  //
  // Unlike data() which returns a copy, returns a reference to the underlying
  // sequence of characters. This can save copying in layout engine where text
  // is broken into words. Should be used carefully to avoid dangling string
  // iterators.
  const std::string& text() const { return data_; }

  DEFINE_WRAPPABLE_TYPE(CharacterData);

 protected:
  CharacterData(Document* document, const base::StringPiece& data);
  ~CharacterData() override {}

 private:
  std::string data_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CHARACTER_DATA_H_
