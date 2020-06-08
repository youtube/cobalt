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

#ifndef COBALT_BROWSER_WEBAPI_EXTENSION_H_
#define COBALT_BROWSER_WEBAPI_EXTENSION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/wrappable.h"

#if SB_API_VERSION < 12

namespace cobalt {
namespace browser {

// This file declares the interface that Cobalt calls in order to possibly
// inject external functionality into the web app's JavaScript environment.
// See cobalt/doc/webapi_extension.md for more information.

// The implementation of this function should return the name of the property
// to be injected into to the JavaScript window object.  If base::nullopt is
// returned, then CreateWebExtensionObject() will not be subsequently called
// and nothing will be injected.
base::Optional<std::string> GetWebAPIExtensionObjectPropertyName();

// The actual object that will be assigned to the window property with name
// given by *GetWebExtensionObjectPropertyName().  The returned object should
// be specified by an IDL interface.  It is passed a reference to |window|
// so that it can access and introspect any properties of the window object.
// It is passed |global_environment| so that it can access functions of the
// GlobalEnvironment interface, such as using it to execute arbitrary
// JavaScript.
scoped_refptr<script::Wrappable> CreateWebAPIExtensionObject(
    const scoped_refptr<dom::Window>& window,
    script::GlobalEnvironment* global_environment);

}  // namespace browser
}  // namespace cobalt

#if defined(COBALT_WEBAPI_EXTENSION_DEFINED)
#pragma message( \
  "Web Extension support is deprecated. Please migrate to Platform Services " \
  "(cobalt/doc/platform_services.md).")
#endif

#elif defined(COBALT_WEBAPI_EXTENSION_DEFINED)

#error \
  "Web Extension support has been deprecated. Please use Platform Services " \
  "(cobalt/doc/platform_services.md) instead."

#endif  // SB_API_VERSION < 12

#endif  // COBALT_BROWSER_WEBAPI_EXTENSION_H_
