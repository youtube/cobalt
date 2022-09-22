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
FROM cobalt-build-win32-base

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';$ProgressPreference='silentlyContinue';"]

ARG RUNNER_VERSION

RUN Invoke-WebRequest -Uri 'https://aka.ms/install-powershell.ps1' -OutFile install-powershell.ps1;`
    powershell -ExecutionPolicy Unrestricted -File ./install-powershell.ps1 -AddToPath

RUN Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v$env:RUNNER_VERSION/actions-runner-win-x64-$env:RUNNER_VERSION.zip -OutFile runner.zip;`
    Expand-Archive -Path C:/runner.zip -DestinationPath C:/actions-runner;`
    Remove-Item -Path C:\runner.zip;`
    setx /M PATH $(${Env:PATH} + \";${Env:ProgramFiles}\Git\bin\")

# Required for packaging artifacts.
RUN choco install -f -y 7zip --version=19.0

ENV TMP "C:\Users\ContainerAdministrator\AppData\Local\Temp2"

ADD runner.ps1 C:/runner.ps1
CMD ["pwsh", "-ExecutionPolicy", "Unrestricted", "-File", ".\\runner.ps1"]
