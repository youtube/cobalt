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

// TODO: b/406082241 - Remove files in starboard/hacks/
// This file is used to stub out any API's/code which is need for building
// upstream chromium code which is theoretically not needed in cobalt. We want
// to revisit all the hacks here and remove them via more elegant methods like
// GN flags, BUILDFLAGS etc.

// ../../third_party/libsync/src/include/sync/sync.h:27:1: error: unknown type
// name '__BEGIN_DECLS'
// ../../third_party/libsync/src/include/sync/sync.h:161:1: error: unknown type
// name '__END_DECLS'
#define __BEGIN_DECLS
#define __END_DECLS

// ../../third_party/libsync/src/sync.c:140:25: error: use of undeclared
// identifier 'SYNC_IOC_FILE_INFO'
//         err = ioctl(fd, SYNC_IOC_FILE_INFO, info);
// ./../third_party/libsync/src/sync.c:126:39: error: expected expression
#define SYNC_IOC_FILE_INFO 0
