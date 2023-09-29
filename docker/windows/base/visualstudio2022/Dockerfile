# escape=`

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
ARG FROM_IMAGE
FROM ${FROM_IMAGE}

SHELL ["powershell", "-ExecutionPolicy", "unrestricted", "-Command"]

COPY ./fast-win-rmdir.cmd /fast-win-rmdir.cmd
COPY ./install_vs_2022.ps1 C:/TEMP/install_vs_2022.ps1

RUN powershell C:/TEMP/install_vs_2022.ps1

ENV VSINSTALLDIR "C:\BuildTools"

ENV VISUAL_STUDIO_VERSION "2022"
