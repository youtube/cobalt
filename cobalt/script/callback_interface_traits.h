// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_CALLBACK_INTERFACE_TRAITS_H_
#define COBALT_SCRIPT_CALLBACK_INTERFACE_TRAITS_H_

namespace cobalt {
namespace script {

// Declare the CallbackInterfaceTraits template struct here. In the generated
// header file for each callback interface there will be an explicit
// instantiation of this template struct which will allow us to infer the type
// of the generated class from the base callback interface type.
template <typename CallbackInterface>
struct CallbackInterfaceTraits {};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_CALLBACK_INTERFACE_TRAITS_H_
