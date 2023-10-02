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
  Installs Visual Studio 2022, with most of the required components for building
  Cobalt. Additionally, removes some unnecessary folders to trim down the final
  image size.
#>

Write-Host 'Downloading vs_buildtools.exe'
Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_buildtools.exe `
  -OutFile C:\TEMP\vs_buildtools.exe

Write-Host 'Installing vs_buildtools.exe'
Start-Process C:\TEMP\vs_buildtools.exe -Wait -NoNewWindow -ArgumentList      `
  '--quiet --wait --norestart --nocache --installPath C:\BuildTools           `
  --add Microsoft.VisualStudio.Component.VC.CoreIde                           `
  --add Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64                `
  --add Microsoft.VisualStudio.Component.VC.Llvm.Clang                        `
  --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset                 `
  --add Microsoft.VisualStudio.Component.Windows10SDK.18362                   `
  --remove Microsoft.VisualStudio.Component.Windows10SDK.10240                `
  --remove Microsoft.VisualStudio.Component.Windows10SDK.10586                `
  --remove Microsoft.VisualStudio.Component.Windows10SDK.14393                `
  --remove Microsoft.VisualStudio.Component.Windows81SDK'

Write-Host 'Cleaning up vs_buildtools.exe'
C:\fast-win-rmdir.cmd ${env:ProgramFiles(x86)}\'Microsoft Visual Studio\Installer'
C:\fast-win-rmdir.cmd $env:TEMP
C:\fast-win-rmdir.cmd $env:ProgramData\'Package Cache'
C:\fast-win-rmdir.cmd C:\BuildTools\VC\Tools\Llvm\ARM64
C:\fast-win-rmdir.cmd C:\BuildTools\Common7\IDE
C:\fast-win-rmdir.cmd C:\TEMP

Exit 0
