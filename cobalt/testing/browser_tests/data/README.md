# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

This directory provides scripts to copy essentially all of the contents of
as a data_dep in cobalt's browser test target, excluding some symlink files.

## Updating Test Data contents

To ensure all necessary test data from `//content/test/data` is included in the `cobalt_browsertests`, the `update_build_gn.py` script located here is used in
a build action. This essentially copies all of `//content/test/data`, with the
exception of files with *symlink* in its name. Modifying this script, and its
associated test, is the recommended way to filter out resource files from the
`cobalt_browsertests` set of `data_deps`.
