/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/css_parser/background_shorthand_property_parse_structures.h"

#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/initial_style.h"

namespace cobalt {
namespace css_parser {

void BackgroundShorthandLayer::ReplaceNullWithInitialValues() {
  if (!background_color) {
    background_color = cssom::GetInitialStyle()->background_color();
  }

  if (!background_image) {
    background_image = cssom::GetInitialStyle()->background_image();
  }

  if (!background_position) {
    background_position = cssom::GetInitialStyle()->background_position();
  }

  if (!background_repeat) {
    background_repeat = cssom::GetInitialStyle()->background_repeat();
  }

  if (!background_size) {
    background_size = cssom::GetInitialStyle()->background_size();
  }
}

bool BackgroundShorthandLayer::IsBackgroundPropertyOverlapped(
    const BackgroundShorthandLayer& that) const {
  if (that.background_color && background_color) {
    return true;
  }

  if (that.background_image && background_image) {
    return true;
  }

  if (that.background_position && background_position) {
    return true;
  }

  if (that.background_repeat && background_repeat) {
    return true;
  }

  if (that.background_size && background_size) {
    return true;
  }

  return false;
}

void BackgroundShorthandLayer::IntegrateNonOverlapped(
    const BackgroundShorthandLayer& that) {
  DCHECK(!IsBackgroundPropertyOverlapped(that));

  if (that.background_color) {
    background_color = that.background_color;
  }

  if (that.background_image) {
    background_image = that.background_image;
  }

  if (that.background_position) {
    background_position = that.background_position;
  }

  if (that.background_repeat) {
    background_repeat = that.background_repeat;
  }

  if (that.background_size) {
    background_size = that.background_size;
  }
}

}  // namespace css_parser
}  // namespace cobalt
