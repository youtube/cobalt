/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/platform/geometry/length.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

PLATFORM_EXPORT DEFINE_GLOBAL(Length, g_auto_length);
PLATFORM_EXPORT DEFINE_GLOBAL(Length, g_none_length);

// static
void Length::Initialize() {
  new (WTF::NotNullTag::kNotNull, (void*)&g_auto_length) Length(kAuto);
  new (WTF::NotNullTag::kNotNull, (void*)&g_none_length) Length(kNone);
}

class CalculationValueHandleMap {
  USING_FAST_MALLOC(CalculationValueHandleMap);

 public:
  CalculationValueHandleMap() = default;
  CalculationValueHandleMap(const CalculationValueHandleMap&) = delete;
  CalculationValueHandleMap& operator=(const CalculationValueHandleMap&) =
      delete;

  int insert(scoped_refptr<const CalculationValue> calc_value) {
    DCHECK(index_);
    // FIXME calc(): https://bugs.webkit.org/show_bug.cgi?id=80489
    // This monotonically increasing handle generation scheme is potentially
    // wasteful of the handle space. Consider reusing empty handles.
    while (map_.Contains(index_))
      index_++;

    map_.Set(index_, std::move(calc_value));

    return index_;
  }

  void Remove(int index) {
    DCHECK(map_.Contains(index));
    map_.erase(index);
  }

  const CalculationValue& Get(int index) {
    DCHECK(map_.Contains(index));
    return *map_.at(index);
  }

  void DecrementRef(int index) {
    DCHECK(map_.Contains(index));
    auto iter = map_.find(index);
    if (iter->value->HasOneRef()) {
      // Force the CalculationValue destructor early to avoid a potential
      // recursive call inside HashMap remove().
      iter->value = nullptr;
      // |iter| may be invalidated during the CalculationValue destructor.
      map_.erase(index);
    } else {
      iter->value->Release();
    }
  }

 private:
  int index_ = 1;
  HashMap<int, scoped_refptr<const CalculationValue>> map_;
};

static CalculationValueHandleMap& CalcHandles() {
  DEFINE_STATIC_LOCAL(CalculationValueHandleMap, handle_map, ());
  return handle_map;
}

Length::Length(scoped_refptr<const CalculationValue> calc)
    : quirk_(false), type_(kCalculated) {
  calculation_handle_ = CalcHandles().insert(std::move(calc));
}

Length Length::BlendMixedTypes(const Length& from,
                               double progress,
                               ValueRange range) const {
  DCHECK(from.IsSpecified());
  DCHECK(IsSpecified());
  return Length(
      AsCalculationValue()->Blend(*from.AsCalculationValue(), progress, range));
}

Length Length::BlendSameTypes(const Length& from,
                              double progress,
                              ValueRange range) const {
  Length::Type result_type = GetType();
  if (IsZero())
    result_type = from.GetType();

  float blended_value = blink::Blend(from.Value(), Value(), progress);
  if (range == ValueRange::kNonNegative)
    blended_value = ClampTo<float>(blended_value, 0);
  return Length(blended_value, result_type);
}

PixelsAndPercent Length::GetPixelsAndPercent() const {
  switch (GetType()) {
    case kFixed:
      return PixelsAndPercent(Value(), 0);
    case kPercent:
      return PixelsAndPercent(0, Value());
    case kCalculated:
      return GetCalculationValue().GetPixelsAndPercent();
    default:
      NOTREACHED();
      return PixelsAndPercent(0, 0);
  }
}

scoped_refptr<const CalculationValue> Length::AsCalculationValue() const {
  if (IsCalculated())
    return &GetCalculationValue();
  return CalculationValue::Create(GetPixelsAndPercent(), ValueRange::kAll);
}

Length Length::SubtractFromOneHundredPercent() const {
  if (IsPercent())
    return Length::Percent(100 - Value());
  DCHECK(IsSpecified());
  scoped_refptr<const CalculationValue> result =
      AsCalculationValue()->SubtractFromOneHundredPercent();
  if (result->IsExpression() ||
      (result->Pixels() != 0 && result->Percent() != 0)) {
    return Length(std::move(result));
  }
  if (result->Percent())
    return Length::Percent(result->Percent());
  return Length::Fixed(result->Pixels());
}

Length Length::Add(const Length& other) const {
  CHECK(IsSpecified());
  if (IsFixed() && other.IsFixed()) {
    return Length::Fixed(Pixels() + other.Pixels());
  }
  if (IsPercent() && other.IsPercent()) {
    return Length::Percent(Percent() + other.Percent());
  }

  scoped_refptr<const CalculationValue> result =
      AsCalculationValue()->Add(*other.AsCalculationValue());
  if (result->IsExpression() ||
      (result->Pixels() != 0 && result->Percent() != 0)) {
    return Length(std::move(result));
  }
  if (result->Percent()) {
    return Length::Percent(result->Percent());
  }
  return Length::Fixed(result->Pixels());
}

Length Length::Zoom(double factor) const {
  switch (GetType()) {
    case kFixed:
      return Length::Fixed(GetFloatValue() * factor);
    case kCalculated:
      return Length(GetCalculationValue().Zoom(factor));
    default:
      return *this;
  }
}

const CalculationValue& Length::GetCalculationValue() const {
  DCHECK(IsCalculated());
  return CalcHandles().Get(CalculationHandle());
}

void Length::IncrementCalculatedRef() const {
  DCHECK(IsCalculated());
  GetCalculationValue().AddRef();
}

void Length::DecrementCalculatedRef() const {
  DCHECK(IsCalculated());
  CalcHandles().DecrementRef(CalculationHandle());
}

float Length::NonNanCalculatedValue(
    float max_value,
    const AnchorEvaluator* anchor_evaluator) const {
  DCHECK(IsCalculated());
  float result = GetCalculationValue().Evaluate(max_value, anchor_evaluator);
  if (std::isnan(result))
    return 0;
  return result;
}

bool Length::IsCalculatedEqual(const Length& o) const {
  return IsCalculated() &&
         (&GetCalculationValue() == &o.GetCalculationValue() ||
          GetCalculationValue() == o.GetCalculationValue());
}

bool Length::HasAnchorQueries() const {
  return IsCalculated() && GetCalculationValue().HasAnchorQueries();
}

bool Length::HasAutoAnchorPositioning() const {
  return IsCalculated() && GetCalculationValue().HasAutoAnchorPositioning();
}

String Length::ToString() const {
  StringBuilder builder;
  builder.Append("Length(");
  static const char* const kTypeNames[] = {
      "Auto",       "Percent",      "Fixed",         "MinContent",
      "MaxContent", "MinIntrinsic", "FillAvailable", "FitContent",
      "Calculated", "ExtendToZoom", "DeviceWidth",   "DeviceHeight",
      "None",       "Content"};
  if (type_ < std::size(kTypeNames))
    builder.Append(kTypeNames[type_]);
  else
    builder.Append("?");
  builder.Append(", ");
  if (IsCalculated()) {
    builder.AppendNumber(calculation_handle_);
  } else {
    builder.AppendNumber(value_);
  }
  if (quirk_)
    builder.Append(", Quirk");
  builder.Append(")");
  return builder.ToString();
}

std::ostream& operator<<(std::ostream& ostream, const Length& value) {
  return ostream << value.ToString();
}

struct SameSizeAsLength {
  int32_t value;
  int32_t meta_data;
};
ASSERT_SIZE(Length, SameSizeAsLength);

}  // namespace blink
