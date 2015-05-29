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

#ifndef CSSOM_TRANSLATE_FUNCTION_H_
#define CSSOM_TRANSLATE_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

// Specifies a translation by the given amount in the X direction.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-translatex
class TranslateXFunction : public TransformFunction {
 public:
  explicit TranslateXFunction(const scoped_refptr<LengthValue>& offset)
      : offset_(offset) {
    DCHECK(offset);
  }

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  const scoped_refptr<LengthValue>& offset() const { return offset_; }

  bool operator==(const TranslateXFunction& other) const {
    return *offset_ == *other.offset_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TranslateXFunction);

 private:
  const scoped_refptr<LengthValue> offset_;

  DISALLOW_COPY_AND_ASSIGN(TranslateXFunction);
};

// Specifies a translation by the given amount in the Y direction.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-translatey
class TranslateYFunction : public TransformFunction {
 public:
  explicit TranslateYFunction(const scoped_refptr<LengthValue>& offset)
      : offset_(offset) {
    DCHECK(offset);
  }

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  const scoped_refptr<LengthValue>& offset() const { return offset_; }

  bool operator==(const TranslateYFunction& other) const {
    return *offset_ == *other.offset_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TranslateYFunction);

 private:
  const scoped_refptr<LengthValue> offset_;

  DISALLOW_COPY_AND_ASSIGN(TranslateYFunction);
};

// Specifies a 3D translation by the vector [0,0,dz] with the given amount
// in the Z direction.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-translatez
class TranslateZFunction : public TransformFunction {
 public:
  explicit TranslateZFunction(const scoped_refptr<LengthValue>& offset)
      : offset_(offset) {
    DCHECK(offset);
  }

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  const scoped_refptr<LengthValue>& offset() const { return offset_; }

  bool operator==(const TranslateZFunction& other) const {
    return *offset_ == *other.offset_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TranslateZFunction);

 private:
  const scoped_refptr<LengthValue> offset_;

  DISALLOW_COPY_AND_ASSIGN(TranslateZFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TRANSLATE_FUNCTION_H_
