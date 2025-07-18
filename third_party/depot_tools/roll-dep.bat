@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.
setlocal

:: Ensure that "depot_tools" is somewhere in PATH so this tool can be used
:: standalone, but allow other PATH manipulations to take priority.
set PATH=%PATH%;%~dp0

:: Defer control.
IF "%GCLIENT_PY3%" == "1" (
  :: Explicitly run on Python 3
  call vpython3 "%~dp0\roll_dep.py" %*
) ELSE IF "%GCLIENT_PY3%" == "0" (
  :: Explicitly run on Python 2
  call vpython "%~dp0\roll_dep.py" %*
) ELSE (
  :: Run on Python 3, allows default to be flipped.
  call vpython3 "%~dp0\roll_dep.py" %*
)
