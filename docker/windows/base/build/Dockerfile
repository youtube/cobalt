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
FROM visual-studio-base

SHELL ["powershell", "-ExecutionPolicy", "unrestricted", "-Command"]

# Helper script for quick delete operations in windows
COPY ./fast-win-rmdir.cmd /fast-win-rmdir.cmd

# Helper script to run an looping command to persist the container instead of
# running the main command.
COPY ./spin.ps1 /spin.ps1

# Helper script to output all the running python processes showing the details
# of the execution, i.e. the full invocation string.
COPY ./list_python_processes.py /list_python_processes.py

# Helper file to ensure python2 is invoked correctly from command line.
COPY ./python2.bat /python2/python2.bat

RUN iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'));`
    # Install deps via chocolatey.
    mkdir C:\choco-cache;`
    choco install -y -c C:\choco-cache python2 --params '/InstallDir:C:\python2';`
    choco install -y -c C:\choco-cache winflexbison3 --params '/InstallDir:C:\bison';`
    choco install -y -c C:\choco-cache ninja;`
    choco install -y -c C:\choco-cache nodejs-lts;`
    choco install -y -c C:\choco-cache git;`
    choco install -y -c C:\choco-cache cmake --installargs 'ADD_CMAKE_TO_PATH=System';`
    Write-Host ('Deleting the chocolately download cache');`
    Remove-Item -Force -Recurse $env:TEMP\*;`
    C:\fast-win-rmdir.cmd C:\choco-cache

RUN mkdir C:\pip-cache;`
    # Install pip.
    pip install pypiwin32 --cache-dir C:\pip-cache;`
    C:\fast-win-rmdir.cmd C:\pip-cache;`
    # Configure git global settings.
    git config --global core.autocrlf false;`
    git config --global core.symlinks false;`
    git config --global core.filemode false;`
    git config --global core.preloadindex true;`
    git config --global core.fscache true;`
    # Registry changes to enable long filesystem paths.
    reg add 'HKLM\SYSTEM\CurrentControlSet\Control\FileSystem' /v LongPathsEnabled /t REG_DWORD /reg:64 /d 1 /f;`
    reg add 'HKLM\SYSTEM\CurrentControlSet\Control\FileSystem' /v LongPathsEnabled /t REG_DWORD /d 1 /f;`
    # Environment variables
    cmd /S /C setx /M python2 'C:\Python2\python.exe';`
    # This fixes a bug running python scripts "LookupError: unknown encoding: cp0".
    cmd /S /C setx /M PYTHONIOENCODING 'UTF-8';`
    # This ensures GYP does not generate PDB files, or create symlinks during build.
    cmd /S /C setx /M IS_DOCKER '1'

# Setup Sccache
ADD https://storage.googleapis.com/cobalt-docker-resources/sccache-cobalt.exe /sccache/sccache.exe
COPY ./setup_sccache.ps1 /sccache/setup_sccache.ps1
ARG encoded_keyfile
RUN C:\sccache\setup_sccache.ps1;`
    Remove-Item C:\sccache\setup_sccache.ps1

# Install gsutil helpers to download from GCS. This helps download required SDKs.
COPY .boto /Users/ContainerAdministrator/.boto
COPY download_from_gcs.ps1 /download_from_gcs.ps1
RUN (New-Object Net.WebClient).DownloadFile(`
    'https://dl.google.com/dl/cloudsdk/channels/rapid/GoogleCloudSDKInstaller.exe',`
    'C:\GoogleCloudSDKInstaller.exe') ; `
    Start-Process -Wait -FilePath "C:\GoogleCloudSDKInstaller.exe" `
    -ArgumentList "/S","/D","C:\gcloud-sdk","/noreporting";`
    Remove-Item -Path C:\GoogleCloudSDKInstaller.exe;`
    cmd /S /C setx BOTO_PATH 'C:\Users\ContainerAdministrator\.boto'

# Configure common env vars.
ENV NINJA_STATUS="[%e sec | %f/%t %u remaining | %c/sec | j%r] "
