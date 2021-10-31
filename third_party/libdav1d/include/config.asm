# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

# NOTE:
# libdav1d's build process will generate this file and populate it with defines
# that configure the project appropriately and set compilation flags. These
# flags are migrated into Cobalt's gyp and ninja build process which makes this
# file superfluous. However, we keep |config.asm| since the file is referenced
# in the includes for several source files and to keep the overall code changes
# low for ease of rebasing upstream changes from libdav1d.
