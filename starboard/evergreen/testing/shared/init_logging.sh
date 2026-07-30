#!/bin/bash
#
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

if [[ ! -d "/tmp/" ]]; then
  log "error" "The '/tmp/' directory is required for log files"
  exit 1
fi

LOG_PATH="/tmp/youtube_test_logs/$(date +%s%3N)"

mkdir -p "${LOG_PATH}" &> /dev/null

if [[ ! -d "${LOG_PATH}" ]]; then
  log "error" "Failed to create directory at '${LOG_PATH}'"
  exit 1
fi
