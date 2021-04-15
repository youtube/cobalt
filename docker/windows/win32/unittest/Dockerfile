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

# Restore the default Windows shell for correct batch processing.
SHELL ["cmd", "/S", "/C"]

RUN powershell -ExecutionPolicy unrestricted -Command `
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

# Needed to extract the app launcher scripts for running tests.
RUN choco install -f -y unzip

# Install python2
RUN choco install -f -y python2 --params '/InstallDir:C:\python2'
RUN pip install pypiwin32
RUN setx path "%path%;C:\Python2"
RUN setx python2 "C:\Python2\python.exe"
ADD scripts/python2.bat /python2/python2.bat

# Install visual C runtime redistributables
ADD https://aka.ms/vs/16/release/vc_redist.x64.exe C:/vcredist_x64.exe
RUN C:/vcredist_x64.exe /install /passive /norestart /log out.txt

ADD scripts/unittest.cmd /unittest.cmd

CMD [ "/unittest.cmd", "${PLATFORM}", "${CONFIG}", "${TEST}"]
