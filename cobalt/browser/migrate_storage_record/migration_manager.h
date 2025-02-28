// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_COBALT_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
#define COBALT_COBALT_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_

#include "content/public/browser/web_contents.h"

namespace cobalt {
namespace migrate_storage_record {

void DoMigrationTasksOnce(content::WebContents* web_contents);

}  // namespace migrate_storage_record
}  // namespace cobalt

#endif  // COBALT_COBALT_MIGRATE_STORAGE_RECORD_MIGRATION_MANAGER_H_
