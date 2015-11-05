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

#include "cobalt/h5vcc/h5vcc_system.h"

namespace cobalt {
namespace h5vcc {

H5vccSystem::H5vccSystem() {}

bool H5vccSystem::are_keys_reversed() const {
  NOTIMPLEMENTED();
  return false;
}

std::string H5vccSystem::build_id() const {
  NOTIMPLEMENTED();
  return "";
}

std::string H5vccSystem::platform() const {
  NOTIMPLEMENTED();
  return "";
}

std::string H5vccSystem::region() const {
  NOTIMPLEMENTED();
  return "";
}

std::string H5vccSystem::version() const {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace h5vcc
}  // namespace cobalt
