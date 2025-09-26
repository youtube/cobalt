// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_LOADER_APP_METRICS_H_
#define STARBOARD_SHARED_STARBOARD_LOADER_APP_METRICS_H_

namespace starboard {

const void* GetLoaderAppMetricsApi();

// Alias to prevent breaking the RDK build on CI.
// See https://paste.googleplex.com/6310485490270208
// TODO: b/441955897 - Remove this alias once RDK build on CI is updated
namespace shared::starboard {
using ::starboard::GetLoaderAppMetricsApi;
}

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_LOADER_APP_METRICS_H_
