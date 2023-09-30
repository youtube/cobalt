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

# Install deps via chocolatey.
COPY ./install_win_deps_choco.ps1 C:/install_win_deps_choco.ps1
RUN C:/install_win_deps_choco.ps1

# Install python3 packages via PIP and set various configurations.
COPY ./requirements.txt /requirements.txt
RUN mkdir C:\pip-cache;`
    python3 -m pip install --require-hashes --no-deps -r /requirements.txt --cache-dir C:\pip-cache;`
    C:\fast-win-rmdir.cmd C:\pip-cache;`
    # Configure git global settings.
    git config --global core.autocrlf false;`
    git config --global core.symlinks false;`
    git config --global core.filemode false;`
    git config --global core.preloadindex true;`
    git config --global core.fscache true;`
    git config --global --add safe.directory C:/code;`
    # Registry changes to enable long filesystem paths.
    reg add 'HKLM\SYSTEM\CurrentControlSet\Control\FileSystem' /v LongPathsEnabled /t REG_DWORD /reg:64 /d 1 /f;`
    reg add 'HKLM\SYSTEM\CurrentControlSet\Control\FileSystem' /v LongPathsEnabled /t REG_DWORD /d 1 /f;`
    # Environment variables
    # This fixes a bug running python scripts "LookupError: unknown encoding: cp0".
    cmd /S /C setx /M PYTHONIOENCODING 'UTF-8';`
    # This ensures GYP does not generate PDB files, or create symlinks during build.
    cmd /S /C setx /M IS_DOCKER '1'

# Setup Sccache
ADD https://storage.googleapis.com/cobalt-docker-resources/sccache-0.2.16-alpha.0.exe /sccache/sccache.exe
RUN mkdir C:\root\sccache;`
    setx /M PATH $($Env:PATH + ';C:\sccache')

# Set up GN
RUN (New-Object Net.WebClient).DownloadFile(`
    'https://chrome-infra-packages.appspot.com/dl/gn/gn/windows-amd64/+/ur-MX9ARZXAVL1MusvU3v4YebmmerRPLDsJQrTLvN1cC',`
    'C:\gn.zip') ; `
    Expand-Archive -Force C:\gn.zip C:\gn\ ; `
    Remove-Item -Path C:\gn.zip ; `
    setx /M PATH $($Env:PATH + ';C:\gn')

# Configure common env vars.
ENV NINJA_STATUS="[%e sec | %f/%t %u remaining | %c/sec | j%r] "
