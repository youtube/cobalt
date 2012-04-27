@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

if [%1]==[] (
  echo 'usage: test-throughput path\to\tests.json [0..n test_index]'
  echo '       run in the same directory as performance_browser_tests'
  exit /b 1
)

for /f "tokens=3" %%i in ('find /c """url"":" %1') do set num_tests=%%i
set /a num_tests-=1
set filter=*ThroughputTest*TestURL

set args1=--gtest_filter=%filter% --gtest_also_run_disabled_tests
set args2=--window-size=1400,1100 --window-position=0,0 --enable-gpu

if [%2]==[] (
  for /L %%G in (0,1,%num_tests%) do (
    performance_browser_tests.exe %args1% %args2% --extra-chrome-flags=%1:%%G
  )
) else (
  performance_browser_tests.exe %args1% %args2% --extra-chrome-flags=%1:%2
)

