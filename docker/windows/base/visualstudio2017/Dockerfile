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

# escape=`
FROM mcr.microsoft.com/windows:1809

# Dockerfile for image used to install Visual Studio.
# WARNING: Changes to this file will result in an extremely long image rebuild.

# Restore the default Windows shell for correct batch processing.
SHELL ["cmd", "/S", "/C"]

# Download the Build Tools bootstrapper.
# This downloads Visual Studio 2017, version 15 (the latest release version).
ADD https://aka.ms/vs/15/release/vs_buildtools.exe C:\TEMP\vs_buildtools.exe

RUN C:\TEMP\vs_buildtools.exe --quiet --wait --norestart --nocache `
    --installPath C:\BuildTools `
    --add Microsoft.VisualStudio.Component.VC.CoreIde `
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    --add Microsoft.VisualStudio.Component.Windows10SDK.17134 `
    --add Microsoft.VisualStudio.Component.Windows10SDK.17763 `
    --add Microsoft.VisualStudio.Component.VC.Tools.14.15' `
 || IF "%ERRORLEVEL%"=="3010" EXIT 0

ENV VSINSTALLDIR=C:\\BuildTools
ENV MSVC_TOOLS_VERSION="14.16.27023"
ENV MSVC_REDIST_VERSION="14.16.27012"

# TODO: remove this environment variable setting, since it is only needed for
# manually deploying debug versions of DLLs. This can be removed in favor of a
# dedicated deployment through gyp/ninja.
ENV MS_WIN_VERSION="10.0.17763.0"
