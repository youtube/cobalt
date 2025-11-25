#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

LCOV_FILE="out/lcov_report/crypto_unittests.lcov"
REPORT_FILE="coverage_report.md"
TARGET_TO_COVERAGE="crypto_unittests"

# 1. Install python dependency
pip install diff-cover

# 2. Run the Chromium coverage tool to generate the LCOV file
#    Ensure the build directory exists and targets are built with coverage flags.
#    The coverage.py script handles the build process internally.
python3 ../../../tools/code_coverage/coverage.py \
  "${TARGET_TO_COVERAGE}" \
  -b "out/coverage" \
  -o "out/lcov_report" \
  -c "out/coverage/${TARGET_TO_COVERAGE}" \
  --coverage-tools-dir "third_party/llvm-build/Release+Asserts/bin" \
  --format=lcov

# Ensure the output directory for the LCOV file exists, and the LCOV file itself is generated.
mkdir -p "$(dirname "${LCOV_FILE}")"

# 3. Run the check_coverage.py script to enforce a 80% threshold on new code
python3 check_coverage.py \
  --lcov-file="${LCOV_FILE}" \
  --compare-branch="origin/main" \
  --threshold=80.0 \
  --report-file="${REPORT_FILE}"

# 4. Echo the location of the generated markdown report
echo "Code coverage report generated at: ${REPORT_FILE}"
