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
if ($LASTEXITCODE == 3010) {
  Write-Host('Exit Code 3010 (Requires Restart) is suppressed')
}

Write-Host ('Cleaning up vs_buildtools.exe')
Remove-Item -Force -Recurse ${env:ProgramFiles(x86)}\'Microsoft Visual Studio'\Installer
Remove-Item -Force -Recurse $env:TEMP\*
Remove-Item -Force -Recurse $env:ProgramData\'Package Cache'\
Remove-Item -Force -Recurse C:\BuildTools\VC\Tools\Llvm\ARM64
Remove-Item -Force -Recurse C:\BuildTools\Common7\IDE
Remove-Item -Force -Recurse C:\TEMP\vs_buildtools.exe

Exit 0
