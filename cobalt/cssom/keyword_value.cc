/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/keyword_value.h"

#include "base/lazy_instance.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

struct KeywordValue::NonTrivialStaticFields {
  NonTrivialStaticFields()
      : auto_value(new KeywordValue(KeywordValue::kAuto)),
        block_value(new KeywordValue(KeywordValue::kBlock)),
        hidden_value(new KeywordValue(KeywordValue::kHidden)),
        inherit_value(new KeywordValue(KeywordValue::kInherit)),
        initial_value(new KeywordValue(KeywordValue::kInitial)),
        inline_value(new KeywordValue(KeywordValue::kInline)),
        inline_block_value(new KeywordValue(KeywordValue::kInlineBlock)),
        none_value(new KeywordValue(KeywordValue::kNone)),
        normal_value(new KeywordValue(KeywordValue::kNormal)),
        visible_value(new KeywordValue(KeywordValue::kVisible)) {}

  const scoped_refptr<KeywordValue> auto_value;
  const scoped_refptr<KeywordValue> block_value;
  const scoped_refptr<KeywordValue> hidden_value;
  const scoped_refptr<KeywordValue> inherit_value;
  const scoped_refptr<KeywordValue> initial_value;
  const scoped_refptr<KeywordValue> inline_value;
  const scoped_refptr<KeywordValue> inline_block_value;
  const scoped_refptr<KeywordValue> none_value;
  const scoped_refptr<KeywordValue> normal_value;
  const scoped_refptr<KeywordValue> visible_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

namespace {

base::LazyInstance<KeywordValue::NonTrivialStaticFields>
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<KeywordValue>& KeywordValue::GetAuto() {
  return non_trivial_static_fields.Get().auto_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetBlock() {
  return non_trivial_static_fields.Get().block_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetHidden() {
  return non_trivial_static_fields.Get().hidden_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInherit() {
  return non_trivial_static_fields.Get().inherit_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInitial() {
  return non_trivial_static_fields.Get().initial_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInline() {
  return non_trivial_static_fields.Get().inline_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInlineBlock() {
  return non_trivial_static_fields.Get().inline_block_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNone() {
  return non_trivial_static_fields.Get().none_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNormal() {
  return non_trivial_static_fields.Get().normal_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetVisible() {
  return non_trivial_static_fields.Get().visible_value;
}

void KeywordValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitKeyword(this);
}

}  // namespace cssom
}  // namespace cobalt
