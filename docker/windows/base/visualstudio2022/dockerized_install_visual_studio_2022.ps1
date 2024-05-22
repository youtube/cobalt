$MSVS_INSTALLER_URL=$args[0]

mkdir C:\TEMP;

Write-Host ('Downloading vs_buildtools.exe');
Invoke-WebRequest -Uri $MSVS_INSTALLER_URL -OutFile C:\TEMP\vs_buildtools.exe;

Write-Host ('Installing vs_buildtools.exe');
Start-Process C:\TEMP\vs_buildtools.exe -Wait -NoNewWindow -ArgumentList @(
  "--quiet", "--wait", "--norestart", "--nocache", "--installPath",
  "C:\BuildTools", "--add", "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
  "--add", "Microsoft.VisualStudio.Component.VC.14.34.17.4.x86.x64",
  "--add", "Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset",
  "--add", "Microsoft.VisualStudio.Component.Windows10SDK.18362");

Write-Host ('Cleaning up installation artifacts');
$FILES_TO_DELETE = @(
  "${env:ProgramFiles(x86)}\'Microsoft Visual Studio'\Installer",
  "C:\TEMP\*",
  "$env:ProgramData\'Package Cache'",
  "C:\BuildTools\Common7\IDE",
  "C:\TEMP\vs_buildtools.exe"
)
Foreach ($file_to_delete in $FILES_TO_DELETE) {
  If (Test-Path $file_to_delete) {
    Remove-Item -Force -Recurse -Path $file_to_delete
  }
}
