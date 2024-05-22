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

# Dockerfile for image used to install Visual Studio.
# WARNING: Changes to this file will result in an extremely long image rebuild.

SHELL ["powershell", "-ExecutionPolicy", "Unrestricted", "-Command", `
       "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

COPY dockerized_install_visual_studio_2022.ps1 /dockerized_install_visual_studio_2022.ps1

# Pins Visual Studio 2022 release version to 17.4.6 (includes Clang 15.0.1).
# This should be kept in sync with the VS installer used in the dev workstation
# setup script in: `tools/install_visual_studio_22.ps1`
ARG MSVS_INSTALLER_URL="https://download.visualstudio.microsoft.com/download/pr/63fee7e3-bede-41ad-97a2-97b8b9f535d1/d4af71832828f344373f7366c2af3461a83569d297dee8b43d30b7da71e9b5ed/vs_BuildTools.exe"
RUN C:\dockerized_install_visual_studio_2022.ps1 ${env:MSVS_INSTALLER_URL}

ENV VSINSTALLDIR "C:\BuildTools"

ENV VISUAL_STUDIO_VERSION "2022"
