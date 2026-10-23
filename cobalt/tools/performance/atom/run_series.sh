#!/usr/bin/env bash
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
#
# ATOM_PERF (PoC): Runs N measurement passes for each cache mode, alternating
# modes, then prints a summary.
#
# Usage: run_series.sh <results_dir> [N=5]
set -euo pipefail

RESULTS_DIR="${1:?Usage: run_series.sh <results_dir> [N]}"
N="${2:-5}"
HERE="$(cd "$(dirname "$0")" && pwd)"

for i in $(seq 1 "${N}"); do
  for mode in empty warm; do
    "${HERE}/run_once.sh" "${RESULTS_DIR}/${mode}/run${i}" "${mode}"
  done
done

for mode in empty warm; do
  echo
  echo "##### cache=${mode}"
  python3 "${HERE}/parse_atom_perf.py" "${RESULTS_DIR}/${mode}"/run*
done | tee "${RESULTS_DIR}/summary.txt"
