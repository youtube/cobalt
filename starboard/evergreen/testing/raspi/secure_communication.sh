#!/bin/bash
#
# Copyright 2022 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# TODO(b/171314488): consider allowing username and password to be passed as
# arguments to run_all_tests.sh.
RASPI_USERNAME="pi"
RASPI_PASSWORD="raspberry"

# The longest timeout for cycling Cobalt is 300 seconds. 360 seconds is used for
# the expect timeout to allow Cobalt to be cycled, with a one minute cushion.
EXPECT_TIMEOUT="360"

function ssh_with_password() {
  expect -c "spawn -noecho ssh ${RASPI_USERNAME}@${RASPI_ADDR} \"$@\"; log_user 0; expect \"password\"; log_user 1; send \"${RASPI_PASSWORD}\\r\"; set timeout ${EXPECT_TIMEOUT};  expect \"eof\""
}

function scp_with_password() {
  expect -c "spawn scp $@; expect \"password\"; send \"${RASPI_PASSWORD}\\r\"; interact"
}

function ssh_with_key() {
  eval "ssh -i ${KEYPATH} pi@${RASPI_ADDR} \"$@\""
}

function scp_with_key() {
  eval "scp -i ${KEYPATH} $@"
}
