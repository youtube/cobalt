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

# Restore the default Windows shell for correct batch processing.
SHELL ["cmd", "/S", "/C"]

RUN powershell -ExecutionPolicy unrestricted -Command `
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

RUN choco install -f -y python2 --params '/InstallDir:C:\python2'
RUN choco install -f -y winflexbison3 --params '/InstallDir:C:\bison'
RUN choco install -f -y ninja
RUN choco install -f -y nodejs-lts
RUN choco install -f -y git
RUN choco install -f -y cmake --installargs 'ADD_CMAKE_TO_PATH=System'

RUN pip install pypiwin32

RUN setx path "%path%;C:\Python2"
RUN setx path "%path%;C:\Python2\Scripts"
RUN setx path "%path%;C:\Python2\Lib"
RUN setx python2 "C:\Python2\python.exe"
ADD ./python2.bat /python2/python2.bat

# This fixes a bug running python scripts "LookupError: unknown encoding: cp0".
ENV PYTHONIOENCODING "UTF-8"

# Registry changes to enable long filesystem paths.
RUN reg add "HKLM\SYSTEM\CurrentControlSet\Control\FileSystem" /v LongPathsEnabled /t REG_DWORD /reg:64 /d 1 /f
RUN reg add "HKLM\SYSTEM\CurrentControlSet\Control\FileSystem" /v LongPathsEnabled /t REG_DWORD /d 1 /f

# Helper script for quick delete operations in windows
COPY ./fast-win-rmdir.cmd /fast-win-rmdir.cmd
# Helper script to run an looping command to persist the container instead of
# running the main command.
COPY ./spin.ps1 /spin.ps1
# Helper script to output all the running python processes showing the details
# of the execution, i.e. the full invocation string.
COPY ./list_python_processes.py /list_python_processes.py

# This env var is read during GYP step and serves two purposes:
#   - Disables PDB generation in win32 and xb1 builds
#   - Disables symlink creation in ps4 deploy build steps
RUN setx IS_DOCKER 1

# Setup Sccache
ADD https://storage.googleapis.com/cobalt-docker-resources/sccache-cobalt.exe /sccache/sccache.exe
COPY ./setup_sccache.ps1 /sccache/setup_sccache.ps1
ARG encoded_gs_service_key_file
ENV ENCODED_GS_SERVICE_KEY_FILE ${encoded_gs_service_key_file}
RUN powershell -ExecutionPolicy Unrestricted -c C:\sccache\setup_sccache.ps1

# Install gsutil helpers to download from GCS
COPY .boto /Users/ContainerAdministrator/.boto
COPY download_from_gcs.ps1 /download_from_gcs.ps1
ADD https://storage.googleapis.com/pub/gsutil.zip /gsutil.zip
RUN powershell Expand-Archive -Force C:\gsutil.zip C:\gsutil
