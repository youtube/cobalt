// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_CONTRIB_STADIA_CLIENTS_VENDOR_PUBLIC_STADIA_PLUGIN_H_
#define STARBOARD_CONTRIB_STADIA_CLIENTS_VENDOR_PUBLIC_STADIA_PLUGIN_H_

#include <stddef.h>
#include <stdint.h>

#include "starboard/contrib/stadia/clients/vendor/public/stadia_export.h"

// An interface header that allows to interact with Stadia plugins.

extern "C" {
typedef struct StadiaPlugin StadiaPlugin;

// Callback that the StadiaPlugin will invoke with data that it needs to pass
// back to the client.
typedef void (*StadiaPluginReceiveFromCallback)(const uint8_t* message,
                                                size_t length,
                                                void* user_data);

STADIA_EXPORT_FUNCTION(bool, StadiaPluginHas, (const char* channel));

STADIA_EXPORT_FUNCTION(StadiaPlugin*,
                       StadiaPluginOpen,
                       (const char* channel,
                        StadiaPluginReceiveFromCallback callback,
                        void* user_data));

// clang-format off
STADIA_EXPORT_FUNCTION(void,
                       StadiaPluginSendTo,
                       (StadiaPlugin* plugin,
                        const char* message,
                        size_t length));

STADIA_EXPORT_FUNCTION(void, StadiaPluginClose, (StadiaPlugin* plugin));
// clang-format on
}
#endif  // STARBOARD_CONTRIB_STADIA_CLIENTS_VENDOR_PUBLIC_STADIA_PLUGIN_H_
