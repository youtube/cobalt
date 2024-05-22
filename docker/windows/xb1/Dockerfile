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

SHELL ["powershell", "-ExecutionPolicy", "Unrestricted", "-Command", `
       "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

# Note: Visual Studio is installed as part of cobalt-build-win-base

# Windows SDK
RUN Invoke-WebRequest -Uri https://go.microsoft.com/fwlink/p/?linkid=2173743 `
    -OutFile C:\win_sdk_installer.exe ; `
    Write-Host 'Installing win_sdk_installer.exe ...' ; `
    C:\win_sdk_installer.exe /ceip off /quiet /features + | Out-Null ; `
    Write-Host 'Install of win_sdk_installer.exe completed. Cleaning up ...' ; `
    Remove-Item -Force -Recurse $env:TEMP\* ; `
    Remove-Item C:\win_sdk_installer.exe

# MSVC sdk
ARG vs_buildtools_version="15"
ARG windows_11_sdk_version="22000"
RUN Write-Host 'Downloading vs_buildtools.exe' ; `
    Invoke-WebRequest -Uri https://aka.ms/vs/${env:vs_buildtools_version}/release/vs_buildtools.exe `
                      -OutFile C:\vs_buildtools.exe ; `
    Write-Host 'Installing vs_buildtools.exe ...' ;`
    Start-Process -Wait -FilePath C:\vs_buildtools.exe -ArgumentList "--quiet","--wait","--norestart","--nocache","--installPath","C:\BuildTools","--add","Microsoft.VisualStudio.Component.Windows11SDK.${env:windows_11_sdk_version}","--add","Microsoft.VisualStudio.Component.VC.Redist.14.Latest" ; `
    Write-Host 'Install of vs_buildtools.exe completed. Cleaning up ...' ; `
    Remove-Item C:\vs_buildtools.exe


WORKDIR "C:\code"

CMD python3 cobalt/build/gn.py -p $env:PLATFORM -c $env:CONFIG out/${env:PLATFORM}_${env:CONFIG};`
    if ($?) {`
      ninja $env:NINJA_FLAGS -C out/${env:PLATFORM}_$env:CONFIG $env:TARGET;`
    }
