// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_screen.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace h5vcc {

// static
float H5vccScreen::GetDiagonal(script::EnvironmentSettings* settings) {
  DCHECK(settings);
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  DCHECK(dom_settings->window());
  return dom_settings->window()->viewport_diagonal_inches();
}

}  // namespace h5vcc
}  // namespace cobalt
