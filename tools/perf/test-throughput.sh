#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ -z "$1" ]
then
  echo 'usage: test-throughput path/to/tests.json [0..n test_index]'
  echo '       run in the same directory as performance_browser_tests'
  exit -1
fi

num_tests=$(grep -o '\"url\"' $1 | wc -l)
filter='*ThroughputTest*TestURL'

args1='--gtest_filter='$filter' --gtest_also_run_disabled_tests'
args2='--window-size=1400,1100 --window-position=0,0 --enable-gpu'

if [ -z "$2" ]
then
  for (( i=0; i<$num_tests; i++ ))
  do
    ./performance_browser_tests $args1 $args2 --extra-chrome-flags=$1:$i
  done
else
  ./performance_browser_tests $args1 $args2 --extra-chrome-flags=$1:$2
fi

