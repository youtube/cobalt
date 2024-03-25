# Helper script to automate installing Visual Studio 22 and required components.

New-Item -Path "C:\" -Name "src" -ItemType "directory" -Force

# Pins Visual Studio 2022 release version to 17.4.6 (includes Clang 15.0.1).
# This should be kept in sync with the VS installer used in Build CI containers
# defined in `docker/windows/base/visualstudio2022/Dockerfile`
$VS_INSTALL_URL="https://download.visualstudio.microsoft.com/download/pr/63fee7e3-bede-41ad-97a2-97b8b9f535d1/2c37061fd2dc51c1283d9e9476437a2b0f211250e514df07c4b9827b95e8d849/vs_Professional.exe"

Invoke-WebRequest -Uri $VS_INSTALL_URL -OutFile C:\src\vs_professional.exe

C:\src\vs_professional.exe --wait --installPath `
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional" `
    --add Microsoft.VisualStudio.Component.VC.CoreIde `
    --add Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64 `
    --add Microsoft.VisualStudio.Component.VC.Llvm.Clang `
    --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset `
    --add Microsoft.VisualStudio.Component.Windows10SDK.18362 `
    --add Microsoft.VisualStudio.Workload.NativeDesktop
