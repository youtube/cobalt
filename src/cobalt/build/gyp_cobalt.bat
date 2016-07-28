@echo off

::
:: Copyright 2014 Google Inc. All Rights Reserved.
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.
::


:: This is a workaround until Cobalt can GYP without using Bash specific
:: commands.

setlocal

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR_UNIX=%SCRIPT_DIR:\=/%

:: Locate depot_tool by searching for the location of git.bat.
for %%F in (git.bat) do set DEPOT_TOOLS_DIR=%%~dp$PATH:F

if "%DEPOT_TOOLS_DIR%" EQU "" (
  echo Can't locate depot_tools.
  goto EOF
)

:: Search for the git directory in depot_tools.
dir /B /A:D %DEPOT_TOOLS_DIR%git-*_bin >%SCRIPT_DIR%git_bin_path.txt 2>NUL
set /P GIT_BIN_DIR=<%SCRIPT_DIR%git_bin_path.txt
del %SCRIPT_DIR%git_bin_path.txt

if "%GIT_BIN_DIR%" EQU "" (
  echo Could not locate git binaries in depot_tools.
  goto EOF
)

:: Full path to the git directory.
set GIT_BIN_DIR=%DEPOT_TOOLS_DIR%%GIT_BIN_DIR%\bin\

:: Convert back slashes into Unix-style forward slashes.
set ARGS=%*
set ARGS_UNIX=%ARGS:\=/%

echo Running gyp_cobalt using git bash, Ctrl+C may not work well...
%GIT_BIN_DIR%bash.exe -lc "python %SCRIPT_DIR_UNIX%gyp_cobalt %ARGS_UNIX%"

:EOF
