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

#include "cobalt/cssom/rgba_color_value.h"

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

// Basic color keywords:
//  https://www.w3.org/TR/css3-color/#html4
// Transparent keyword:
//  https://www.w3.org/TR/css3-color/#transparent-def
uint32 kAqua = 0x00FFFFFF;
uint32 kBlack = 0x000000FF;
uint32 kBlue = 0x0000FFFF;
uint32 kFuchsia = 0xFF00FFFF;
uint32 kGray = 0x808080FF;
uint32 kGreen = 0x008000FF;
uint32 kLime = 0x00FF00FF;
uint32 kMaroon = 0x800000FF;
uint32 kNavy = 0x000080FF;
uint32 kOlive = 0x808000FF;
uint32 kPurple = 0x800080FF;
uint32 kRed = 0xFF0000FF;
uint32 kSilver = 0xC0C0C0FF;
uint32 kTeal = 0x008080FF;
uint32 kTransparent = 0x00000000;
uint32 kWhite = 0xFFFFFFFF;
uint32 kYellow = 0xFFFF00FF;

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : aqua_value(new RGBAColorValue(kAqua)),
        black_value(new RGBAColorValue(kBlack)),
        blue_value(new RGBAColorValue(kBlue)),
        fuchsia_value(new RGBAColorValue(kFuchsia)),
        gray_value(new RGBAColorValue(kGray)),
        green_value(new RGBAColorValue(kGreen)),
        lime_value(new RGBAColorValue(kLime)),
        maroon_value(new RGBAColorValue(kMaroon)),
        navy_value(new RGBAColorValue(kNavy)),
        olive_value(new RGBAColorValue(kOlive)),
        purple_value(new RGBAColorValue(kPurple)),
        red_value(new RGBAColorValue(kRed)),
        silver_value(new RGBAColorValue(kSilver)),
        teal_value(new RGBAColorValue(kTeal)),
        transparent_value(new RGBAColorValue(kTransparent)),
        white_value(new RGBAColorValue(kWhite)),
        yellow_value(new RGBAColorValue(kYellow)) {}

  const scoped_refptr<RGBAColorValue> aqua_value;
  const scoped_refptr<RGBAColorValue> black_value;
  const scoped_refptr<RGBAColorValue> blue_value;
  const scoped_refptr<RGBAColorValue> fuchsia_value;
  const scoped_refptr<RGBAColorValue> gray_value;
  const scoped_refptr<RGBAColorValue> green_value;
  const scoped_refptr<RGBAColorValue> lime_value;
  const scoped_refptr<RGBAColorValue> maroon_value;
  const scoped_refptr<RGBAColorValue> navy_value;
  const scoped_refptr<RGBAColorValue> olive_value;
  const scoped_refptr<RGBAColorValue> purple_value;
  const scoped_refptr<RGBAColorValue> red_value;
  const scoped_refptr<RGBAColorValue> silver_value;
  const scoped_refptr<RGBAColorValue> teal_value;
  const scoped_refptr<RGBAColorValue> transparent_value;
  const scoped_refptr<RGBAColorValue> white_value;
  const scoped_refptr<RGBAColorValue> yellow_value;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

namespace {

base::LazyInstance<NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetAqua() {
  return non_trivial_static_fields.Get().aqua_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetBlack() {
  return non_trivial_static_fields.Get().black_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetBlue() {
  return non_trivial_static_fields.Get().blue_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetFuchsia() {
  return non_trivial_static_fields.Get().fuchsia_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetGray() {
  return non_trivial_static_fields.Get().gray_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetGreen() {
  return non_trivial_static_fields.Get().green_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetLime() {
  return non_trivial_static_fields.Get().lime_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetMaroon() {
  return non_trivial_static_fields.Get().maroon_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetNavy() {
  return non_trivial_static_fields.Get().navy_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetOlive() {
  return non_trivial_static_fields.Get().olive_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetPurple() {
  return non_trivial_static_fields.Get().purple_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetRed() {
  return non_trivial_static_fields.Get().red_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetSilver() {
  return non_trivial_static_fields.Get().silver_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetTeal() {
  return non_trivial_static_fields.Get().teal_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetTransparent() {
  return non_trivial_static_fields.Get().transparent_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetWhite() {
  return non_trivial_static_fields.Get().white_value;
}

const scoped_refptr<RGBAColorValue>& RGBAColorValue::GetYellow() {
  return non_trivial_static_fields.Get().yellow_value;
}

void RGBAColorValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitRGBAColor(this);
}

std::string RGBAColorValue::ToString() const {
  if (a() >= 255.0) {
    return base::StringPrintf("rgb(%u, %u, %u)", r(), g(), b());
  } else {
    return base::StringPrintf("rgba(%u, %u, %u, %.7g)", r(), g(), b(),
                              a() / 255.0);
  }
}

}  // namespace cssom
}  // namespace cobalt
