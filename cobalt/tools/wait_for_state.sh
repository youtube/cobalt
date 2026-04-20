#!/bin/bash
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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


EXPR=$1
EXPECTED=$2
PORT=$3
TIMEOUT=${4:-10}

START_TIME=$(date +%s)

echo "[WAIT] Waiting for '$EXPR' to be '$EXPECTED' (timeout: ${TIMEOUT}s)..."

while true; do
  RESULT=$(vpython3 cobalt/tools/cdp_js_helper.py --port $PORT "$EXPR" 2>/dev/null)
  if [ "$RESULT" == "$EXPECTED" ]; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    echo "[WAIT] SUCCESS: '$EXPR' is now '$EXPECTED' (took ${DURATION}s)"
    exit 0
  fi

  CURRENT_TIME=$(date +%s)
  if [ $((CURRENT_TIME - START_TIME)) -gt $TIMEOUT ]; then
    echo "FAILURE: Timeout waiting for $EXPR to be $EXPECTED. Got: $RESULT"
    exit 1
  fi
  sleep 0.5
done
