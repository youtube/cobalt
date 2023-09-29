Write-Host ('Downloading vs_buildtools.exe')
Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_buildtools.exe `
  -OutFile C:\TEMP\vs_buildtools.exe

Write-Host ('Installing vs_buildtools.exe')
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

if (!$?) { Exit 1 }
if ($LASTEXITCODE -eq 3010) {
  Write-Host('Exit Code 3010 (Requires Restart) is suppressed')
}

Write-Host ('Cleaning up vs_buildtools.exe')
C:\fast-win-rmdir.cmd '${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer'
C:\fast-win-rmdir.cmd $env:TEMP
C:\fast-win-rmdir.cmd '$env:ProgramData\Package Cache'
C:\fast-win-rmdir.cmd C:\BuildTools\VC\Tools\Llvm\ARM64
C:\fast-win-rmdir.cmd C:\BuildTools\Common7\IDE
C:\fast-win-rmdir.cmd C:\TEMP

Exit 0
