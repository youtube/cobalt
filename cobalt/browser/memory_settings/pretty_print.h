/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_PRETTY_PRINT_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_PRETTY_PRINT_H_

#include <string>
#include <vector>

#include "cobalt/browser/memory_settings/memory_settings.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// Generates a table, ie:
//    "NAME                                   VALUE       SOURCE          \n"
//    "+--------------------------------------+-----------+--------------+\n"
//    "| image_cache_size_in_bytes            |      1234 |      CmdLine |\n"
//    "+--------------------------------------+-----------+--------------+\n"
//    "| javascript_gc_threshold_in_bytes     |      1112 |      AutoSet |\n"
//    "+--------------------------------------+-----------+--------------+\n"
//    "| skia_atlas_texture_dimensions        | 1234x4567 |      CmdLine |\n"
//    "+--------------------------------------+-----------+--------------+\n"
//    "| skia_cache_size_in_bytes             |         0 |          N/A |\n"
//    "+--------------------------------------+-----------+--------------+\n"
//    "| software_surface_cache_size_in_bytes |      8910 | BuildSetting |\n"
//    "+--------------------------------------+-----------+--------------+\n";
std::string GeneratePrettyPrintTable(
    const std::vector<const MemorySetting*>& memory_settings);

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_PRETTY_PRINT_H_
