# Helper script to automate installing Visual Studio 22 and required components.

New-Item -Path "C:\" -Name "src" -ItemType "directory" -Force

$VS_INSTALL_URL="https://download.visualstudio.microsoft.com/download/pr/d1ed8638-9e88-461e-92b7-4e29cc6172c3/30be9c2a5e40d5666eeae683e319b472c8bcc8231c7b346fe798f0ad0f7e498b/vs_Professional.exe"

Invoke-WebRequest -Uri $VS_INSTALL_URL -OutFile C:\src\vs_professional.exe

C:\src\vs_professional.exe --wait --installPath `
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional" `
    --add Microsoft.VisualStudio.Component.VC.CoreIde `
    --add Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64 `
    --add Microsoft.VisualStudio.Component.VC.Llvm.Clang `
    --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset `
    --add Microsoft.VisualStudio.Component.Windows10SDK.18362 `
    --add Microsoft.VisualStudio.Workload.NativeDesktop
