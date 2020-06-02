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

#include "cobalt/dom/dom_token_list.h"

#include <algorithm>

#include "base/strings/string_split.h"
#include "cobalt/dom/global_stats.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

DOMTokenList::DOMTokenList(Element* element, const std::string& attr_name)
    : element_(element),
      attr_name_(attr_name),
      element_node_generation_(Node::kInvalidNodeGeneration) {
  TRACK_MEMORY_SCOPE("DOM");
  DCHECK(element);
  // The current implementation relies on nodes calling UpdateNodeGeneration()
  // each time the class is changed. This results in DOMTokenList only working
  // for class attribute. DOMTokenList is only used by Element::class_list(),
  // and it is not likely to be used anywhere else. Therefore DCHECK is used to
  // guarantee attr_name is always "class".
  DCHECK_EQ(attr_name, "class");
  GlobalStats::GetInstance()->Add(this);
}

// Algorithm for length:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-length
unsigned int DOMTokenList::length() const {
  // Custom, not in any spec.
  MaybeRefresh();

  return static_cast<unsigned int>(tokens_.size());
}

// Algorithm for Item:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-item
base::Optional<std::string> DOMTokenList::Item(unsigned int index) const {
  // Custom, not in any spec.
  MaybeRefresh();

  // 1. If index is equal to or greater than the number of tokens in tokens,
  //    return null.
  if (index >= tokens_.size()) {
    return base::nullopt;
  }

  // 2. Return the indexth token in tokens.
  return std::string(tokens_[index].c_str());
}

// Algorithm for Contains:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-contains
bool DOMTokenList::Contains(const std::string& token) const {
  // Custom, not in any spec.
  MaybeRefresh();

  // Cobalt changes the spec processing order to 3, then 1 and 2. The reason for
  // this is that we're guaranteed that if the token is in the list, it is
  // valid, and it being in the list is far more likely to be the case than it
  // being invalid. Given that the two states are mutually exclusive, we're
  // processing the likeliest path first.

  // 3. Return true if token is in tokens, and false otherwise.
  if (std::find(tokens_.begin(), tokens_.end(), token) != tokens_.end()) {
    return true;
  }

  // 1. If token is the empty string, then throw a "SyntaxError" exception.
  // 2. If token contains any ASCII whitespace, then throw an
  //    "InvalidCharacterError" exception.
  if (!IsTokenValid(token)) {
    return false;
  }

  return false;
}

// Algorithm for Add:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-add
void DOMTokenList::Add(const std::vector<std::string>& tokens) {
  TRACK_MEMORY_SCOPE("DOM");
  // Custom, not in any spec.
  MaybeRefresh();

  for (std::vector<std::string>::const_iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    // 1. If token is the empty string, then throw a "SyntaxError" exception.
    // 2. If token contains any ASCII whitespace, then throw an
    //    "InvalidCharacterError" exception.
    if (!IsTokenValid(*it)) {
      return;
    }
  }

  for (std::vector<std::string>::const_iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    // 3. For each token in tokens, in given order, that is not in tokens,
    //    append token to tokens.
    if (std::find(tokens_.begin(), tokens_.end(), *it) != tokens_.end()) {
      continue;
    }
    tokens_.push_back(base::Token(*it));
  }

  // 4. Run the update steps.
  RunUpdateSteps();
}

// Algorithm for Remove:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-remove
void DOMTokenList::Remove(const std::vector<std::string>& tokens) {
  // Custom, not in any spec.
  MaybeRefresh();

  for (std::vector<std::string>::const_iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    // 1. If token is the empty string, then throw a "SyntaxError" exception.
    // 2. If token contains any ASCII whitespace, then throw an
    //    "InvalidCharacterError" exception.
    if (!IsTokenValid(*it)) {
      return;
    }
  }

  for (std::vector<std::string>::const_iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    // 3. For each token in tokens, remove token from tokens.
    tokens_.erase(std::remove(tokens_.begin(), tokens_.end(), *it),
                  tokens_.end());
  }

  // 4. Run the update steps.
  RunUpdateSteps();
}

// Algorithm for Toggle:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-toggle
bool DOMTokenList::Toggle(const std::string& token,
                          const base::Optional<bool>& force) {
  // Custom, not in any spec.
  MaybeRefresh();

  // If token is the empty string, then throw a "SyntaxError" exception.
  // If token contains any ASCII whitespace, then throw an
  // "InvalidCharacterError" exception.
  if (!IsTokenValid(token)) {
    return false;
  }

  bool containsToken = ContainsValid(base::Token(token));

  // If token in tokens, and force is true, return true.
  // If token not in tokens, and force is false, return false.
  if (containsToken == force) {
    return containsToken;
  }

  // If token in tokens, then remove token from tokens.
  // If token not in tokens, append token to tokens.
  if (containsToken) {
    tokens_.erase(std::remove(tokens_.begin(), tokens_.end(), token),
                  tokens_.end());
  } else {
    tokens_.push_back(base::Token(token));
  }

  // Run the update steps.
  RunUpdateSteps();

  // Return true if token was appended to tokens.
  // Return false if token was removed from tokens.
  return !containsToken;
}

// Algorithm for AnonymousStringifier:
//   https://www.w3.org/TR/dom/#dom-domtokenlist-stringifier
std::string DOMTokenList::AnonymousStringifier() const {
  // Custom, not in any spec.
  MaybeRefresh();

  std::string result;
  for (size_t i = 0; i < tokens_.size(); ++i) {
    result += tokens_[i].c_str();
    result += ' ';
  }
  if (!result.empty()) {
    result.resize(result.size() - 1);
  }
  return result;
}

bool DOMTokenList::ContainsValid(base::Token valid_token) const {
  MaybeRefresh();

  // This version of Contains does not process steps 1 and 2 and requires the
  // token to be pre-validated.

  // 1. If token is the empty string, then throw a "SyntaxError" exception.
  // 2. If token contains any ASCII whitespace, then throw an
  //    "InvalidCharacterError" exception.

  // 3. Return true if token is in tokens, and false otherwise.
  if (std::find(tokens_.begin(), tokens_.end(), valid_token) != tokens_.end()) {
    return true;
  }

  return false;
}

const std::vector<base::Token>& DOMTokenList::GetTokens() const {
  MaybeRefresh();
  return tokens_;
}

void DOMTokenList::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(element());
}

DOMTokenList::~DOMTokenList() { GlobalStats::GetInstance()->Remove(this); }

// Algorithm for RunUpdateSteps:
//   https://www.w3.org/TR/dom/#concept-dtl-update
void DOMTokenList::RunUpdateSteps() const {
  TRACK_MEMORY_SCOPE("DOM");
  // 1. If there is no associated attribute (when the object is a
  // DOMSettableTokenList), terminate these steps.
  // 2. Set an attribute for the associated element using associated attribute's
  // local name and the result of running the ordered set serializer for tokens.
  element_->SetAttribute(attr_name_, AnonymousStringifier());
}

bool DOMTokenList::IsTokenValid(const std::string& token) const {
  if (token.empty()) {
    // TODO: Throw JS SyntaxError.
    return false;
  }
  if (token.find_first_of(" \n\t\r\f") != std::string::npos) {
    // TODO: Throw JS InvalidCharacterError.
    return false;
  }
  return true;
}

void DOMTokenList::MaybeRefresh() const {
  TRACK_MEMORY_SCOPE("DOM");
  if (element_node_generation_ != element_->node_generation()) {
    element_node_generation_ = element_->node_generation();
    std::string attribute = element_->GetAttribute(attr_name_).value_or("");
    std::vector<std::string> tokens =
        base::SplitString(attribute, base::kWhitespaceASCII,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    tokens_.clear();
    tokens_.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
      tokens_.push_back(base::Token(tokens[i]));
    }
  }
}

}  // namespace dom
}  // namespace cobalt
