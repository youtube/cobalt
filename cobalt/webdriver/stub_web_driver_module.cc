// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// A source file used to create a nearly empty library that is needed for
// dependency purposes. On Windows one cannot create a library that does not
// define any previously undefined public symbols.
// See related Linker Tools Warning LNK4221:
// https://msdn.microsoft.com/en-us/library/604bzebd.aspx

namespace cobalt {
namespace webdriver {
class WebDriverModule {};
}  // namespace webdriver
}  // namespace cobalt
