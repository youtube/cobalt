# Helper script to automate installing Visual Studio 22 and required components.

New-Item -Path "C:\" -Name "src" -ItemType "directory" -Force

Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_professional.exe `
    -OutFile C:\src\vs_professional.exe

C:\src\vs_professional.exe --wait --installPath `
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional" `
    --add Microsoft.VisualStudio.Component.VC.CoreIde `
    --add Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64 `
    --add Microsoft.VisualStudio.Component.VC.Llvm.Clang `
    --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset `
    --add Microsoft.VisualStudio.Component.Windows10SDK.18362 `
    --add Microsoft.VisualStudio.Workload.NativeDesktop
