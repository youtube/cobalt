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

function ssh_with_password() {
  expect <<EOD
# The longest timeout for cycling Cobalt is 300 seconds. 360 seconds is used for
# the expect timeout to allow Cobalt to be cycled, with a one minute cushion.
set timeout 360
spawn -noecho ssh ${RASPI_USERNAME}@${RASPI_ADDR} "$@"
log_user 0
expect {
    "password:" {
        log_user 1
        send "${RASPI_PASSWORD}\r"
        expect {
            timeout {
                puts stderr "\n expect command timed out after sending password"
                exit 1
            } eof {}
        }
    } timeout {
        puts stderr "\n expect timed out waiting for password prompt"
        exit 1
    } eof {
        puts stderr "received eof from spawn command"
        exit 1
    }
}
EOD

  if [[ $? -eq 1 ]]; then
    echo "ssh_with_password() failed, exiting" 1>&2
    exit 1
  fi
}

function scp_with_password() {
  expect <<EOD
# Some directories, e.g., cobalt/content, may take a few minutes to copy.
set timeout 360
spawn scp $@
expect {
    "password:" {
        send "${RASPI_PASSWORD}\r"
        expect {
            timeout {
                puts stderr "\n expect timed out after sending password"
                exit 1
            } eof {}
        }
    } timeout {
        puts stderr "\n expect timed out waiting for password prompt"
        exit 1
    } eof {
        puts stderr "received eof from spawn command"
        exit 1
    }
}
EOD

  if [[ $? -eq 1 ]]; then
    echo "scp_with_password() failed, exiting" 1>&2
    exit 1
  fi
}

function ssh_with_key() {
  eval "ssh -i ${KEYPATH} pi@${RASPI_ADDR} \"$@\""
}

function scp_with_key() {
  eval "scp -i ${KEYPATH} $@"
}
