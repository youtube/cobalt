# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

FROM build-base-windows

# This is necessary in order to ensure that gyp configures the build without PDB
# generation.
# PDB files cannot be created inside of volumes due to limitations with windows
# docker engine and our current in-source build.
RUN setx is_docker 1

ADD scripts/build.cmd /build.cmd
ADD scripts/fast-win-rmdir.cmd /fast-win-rmdir.cmd

CMD [ "/build.cmd", "${PLATFORM}", "${CONFIG}", "${TARGET}", "${NINJA_FLAGS}" ]
