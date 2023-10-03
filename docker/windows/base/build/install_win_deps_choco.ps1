# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
<#
.DESCRIPTION
  Install required Cobalt build dependencies from chocolatey.
#>

# Pin Choco to 1.4.0 to avoid required reboot in 2.0.0
$env:chocolateyVersion = '1.4.0'

iex ((New-Object System.Net.WebClient).DownloadString( `
      'https://chocolatey.org/install.ps1'))`

mkdir C:\choco-cache

# Note: We pinned python 3.11.0 version here because 3.11.2 has a regression
# where the install arguments like TargetDir were being ignored.
choco install -y -c C:\choco-cache python3 --version 3.11.0 -ia `
  '/quiet InstallAllUsers=1 PrependPath=1 TargetDir="C:\Python3"'
choco install -y -c C:\choco-cache winflexbison3 --params '/InstallDir:C:\bison'
choco install -y -c C:\choco-cache ninja
choco install -y -c C:\choco-cache nodejs-lts
choco install -y -c C:\choco-cache git
choco install -y -c C:\choco-cache cmake -ia 'ADD_CMAKE_TO_PATH=System'

Write-Host ('Deleting the chocolately download cache')
C:\fast-win-rmdir.cmd $env:TEMP
C:\fast-win-rmdir.cmd C:\choco-cache

Copy-Item C:\Python3\python.exe C:\Python3\python3.exe
