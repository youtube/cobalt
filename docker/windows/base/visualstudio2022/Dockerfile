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

ARG MSVS_INSTALLER_URL="https://download.visualstudio.microsoft.com/download/pr/63b5064f-af60-4cbe-96cd-a9dd9d41ee3d/436695f5b9083e37a62f63326dd4c2757b5780316f2c7e6ba0fdcee0770bfe67/vs_BuildTools.exe"
RUN mkdir C:\TEMP;`
    Write-Host ('Downloading vs_buildtools.exe');`
    Invoke-WebRequest -Uri ${MSVS_INSTALLER_URL} `
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
    Remove-Item -Force -Recurse C:\BuildTools\VC\Tools\Llvm\ARM64;`
    Remove-Item -Force -Recurse C:\BuildTools\Common7\IDE;`
    Remove-Item C:\TEMP\vs_buildtools.exe

ENV VSINSTALLDIR "C:\BuildTools"

ENV VISUAL_STUDIO_VERSION "2022"
