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
#include "base/threading/thread_checker.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

const char* const kAutoKeywordName = "auto";
const char* const kBlockKeywordName = "block";
const char* const kHiddenKeywordName = "hidden";
const char* const kInheritKeywordName = "inherit";
const char* const kInitialKeywordName = "initial";
const char* const kInlineBlockKeywordName = "inline-block";
const char* const kInlineKeywordName = "inline";
const char* const kNoneKeywordName = "none";
const char* const kVisibleKeywordName = "visible";

namespace {

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : auto_value(new KeywordValue(KeywordValue::kAuto)),
        block_value(new KeywordValue(KeywordValue::kBlock)),
        inherit_value(new KeywordValue(KeywordValue::kInherit)),
        initial_value(new KeywordValue(KeywordValue::kInitial)),
        inline_value(new KeywordValue(KeywordValue::kInline)),
        inline_block_value(new KeywordValue(KeywordValue::kInlineBlock)),
        none_value(new KeywordValue(KeywordValue::kNone)) {}

  base::ThreadChecker thread_checker;

  const scoped_refptr<KeywordValue> auto_value;
  const scoped_refptr<KeywordValue> block_value;
  const scoped_refptr<KeywordValue> hidden_value;
  const scoped_refptr<KeywordValue> inherit_value;
  const scoped_refptr<KeywordValue> initial_value;
  const scoped_refptr<KeywordValue> inline_value;
  const scoped_refptr<KeywordValue> inline_block_value;
  const scoped_refptr<KeywordValue> none_value;
  const scoped_refptr<KeywordValue> visible_value;
};

base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<KeywordValue>& KeywordValue::GetAuto() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().auto_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetBlock() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().block_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInherit() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().inherit_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInitial() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().initial_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInline() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().inline_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetInlineBlock() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().inline_block_value;
}

const scoped_refptr<KeywordValue>& KeywordValue::GetNone() {
  DCHECK(non_trivial_static_fields.Get().thread_checker.CalledOnValidThread());
  return non_trivial_static_fields.Get().none_value;
}

void KeywordValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitKeyword(this);
}

}  // namespace cssom
}  // namespace cobalt
