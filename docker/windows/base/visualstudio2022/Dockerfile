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

# Pins Visual Studio 2022 release version to 17.4.6 (includes Clang 15.0.1).
# This should be kept in sync with the VS installer used in the dev workstation
# setup script in: `tools/install_visual_studio_22.ps1`
ARG MSVS_INSTALLER_URL="https://download.visualstudio.microsoft.com/download/pr/d1ed8638-9e88-461e-92b7-4e29cc6172c3/38b09fc09ae9e590b73ae6752a0ebfd62579798969041bd341689273b842bc10/vs_BuildTools.exe"
RUN mkdir C:\TEMP;`
    Write-Host ('Downloading vs_buildtools.exe');`
    Invoke-WebRequest -Uri ${env:MSVS_INSTALLER_URL} `
      -OutFile C:\TEMP\vs_buildtools.exe

RUN Write-Host ('Installing vs_buildtools.exe');`
    Start-Process C:\TEMP\vs_buildtools.exe -Wait -NoNewWindow`
        -ArgumentList '--quiet --wait --norestart --nocache`
        --installPath C:\BuildTools `
        --add Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64  `
        --add Microsoft.VisualStudio.Component.VC.Llvm.Clang          `
        --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset   `
        --add Microsoft.VisualStudio.Component.Windows10SDK.18362';   `
    Write-Host ('Cleaning up vs_buildtools.exe');`
    Remove-Item -Force -Recurse ${env:ProgramFiles(x86)}\'Microsoft Visual Studio'\Installer;`
    Remove-Item -Force -Recurse $env:TEMP\*;`
    Remove-Item -Force -Recurse $env:ProgramData\'Package Cache'\;`
    Remove-Item -Force -Recurse C:\BuildTools\Common7\IDE;`
    Remove-Item C:\TEMP\vs_buildtools.exe

ENV VSINSTALLDIR "C:\BuildTools"

ENV VISUAL_STUDIO_VERSION "2022"
