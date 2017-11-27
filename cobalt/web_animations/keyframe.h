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

#ifndef COBALT_WEB_ANIMATIONS_KEYFRAME_H_
#define COBALT_WEB_ANIMATIONS_KEYFRAME_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/small_map.h"
#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/timing_function.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace web_animations {

// Individual keyframes are represented by a special kind of Keyframe dictionary
// type whose members map to the properties to be animated.
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-keyframe-dictionary
// A Keyframe associates target values for a set of CSS properties to a
// particular normalized offset between 0 and 1.
class Keyframe : public script::Wrappable {
 public:
  // Contains all data associated with a Keyframe.  As an object separate from
  // Keyframe, it can be extracted out of a Keyframe object and copied around
  // externally as needed.
  class Data {
   public:
    Data() : easing_(cssom::TimingFunction::GetLinear()) {}

    explicit Data(double offset)
        : offset_(offset), easing_(cssom::TimingFunction::GetLinear()) {}

    Data(double offset, const scoped_refptr<cssom::TimingFunction>& easing)
        : offset_(offset), easing_(easing) {}

    const base::optional<double>& offset() const { return offset_; }
    void set_offset(const base::optional<double>& offset) { offset_ = offset; }

    const scoped_refptr<cssom::TimingFunction>& easing() const {
      return easing_;
    }
    void set_easing(const scoped_refptr<cssom::TimingFunction>& easing) {
      easing_ = easing;
    }

    // A Keyframe is specified to be a dictionary, not an interface, so the
    // property names and values are simply specified as additional key/value
    // entries into the Keyframe dictionary, on top of 'offset' and 'easing'.
    // Here though, we explicitly introduce a map for property names to
    // property values at this keyframe.
    typedef base::SmallMap<
        std::map<cssom::PropertyKey, scoped_refptr<cssom::PropertyValue> >, 2>
        PropertyValueMap;
    const PropertyValueMap& property_values() const { return property_values_; }

    bool AffectsProperty(cssom::PropertyKey property) const {
      return property_values_.find(property) != property_values_.end();
    }

    void AddPropertyValue(cssom::PropertyKey property,
                          scoped_refptr<cssom::PropertyValue> property_value) {
      property_values_.insert(std::make_pair(property, property_value));
    }

   private:
    base::optional<double> offset_;
    scoped_refptr<cssom::TimingFunction> easing_;
    PropertyValueMap property_values_;
  };

  Keyframe() {}

  const base::optional<double>& offset() const { return data_.offset(); }
  void set_offset(const base::optional<double>& offset) {
    data_.set_offset(offset);
  }

  std::string easing() const {
    NOTIMPLEMENTED();
    return std::string("linear");
  }
  void set_easing(const std::string& easing) {
    UNREFERENCED_PARAMETER(easing);
    NOTIMPLEMENTED();
  }

  // Custom, not in any spec.
  const Data& data() const { return data_; }

  DEFINE_WRAPPABLE_TYPE(Keyframe);

 private:
  ~Keyframe() override {}

  Data data_;

  DISALLOW_COPY_AND_ASSIGN(Keyframe);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_KEYFRAME_H_
