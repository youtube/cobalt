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

This provides a blank data directory that the `cobalt_browsertests` target depends on to run successfully.

## Updating Test Data

To ensure all necessary test data from `//content/test/data` is included in the `cobalt_browsertests`, run the `update_build_gn.py` script located here.

```bash
python3 cobalt/cobalt/testing/browser_tests/data/update_build_gn.py
```

This script will automatically populate the `data` array in `BUILD.gn` with all files found in `//content/test/data`. It should be run whenever the `content/test/data` directory changes or after rebasing the repository.

Note: Cobalt browser tests should exclude tests with symlinks.
