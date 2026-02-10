#!/usr/bin/env vpython3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Regex patterns for Cobalt build and run log analysis."""

import re

# --- Log Analysis Patterns ---

# Regex to detect the start of a gtest, used for log type detection
GTEST_RUN_PATTERN = re.compile(r'(?:\x1B\[[0-9;]*m)?\s*\[ RUN      \]\s*(?:\x1B\[m)?\s*(.*)')

STACK_TRACE_LINE_PATTERN = re.compile(
    r'(\x1B\[[0-9;]*m)?(\[0x[0-9a-fA-F]+\]|#\d+\s| at .*\()|:[0-9]+(\)|$)'
)

TIMESTAMP_PATTERN = re.compile(r'\[\w+/\d+:\d+/\d+\.\d+\(UTC\):')

# Gtest result patterns
GTEST_OK_PATTERN = re.compile(r'(?:\x1B\[[0-9;]*m)?\s*\[       OK \]\s*(?:\x1B\[m)?\s*(.*)')
GTEST_FAILED_PATTERN = re.compile(r'(?:\x1B\[[0-9;]*m)?\s*\[  FAILED  \]\s*(?:\x1B\[m)?\s*(.*\..*)')
GTEST_LOG_PATTERN = re.compile(r'(?:\x1B\[[0-9;]*m)?\s*\[ INFO     \]\s*(?:\x1B\[m)?\s*(.*)')
GTEST_SKIPPED_PATTERN = re.compile(r'(?:\x1B\[[0-9;]*m)?\s*\[  SKIPPED \]\s*(?:\x1B\[m)?\s*(.*)')

# Issue patterns for test logs
ISSUE_PATTERNS = {
    'crash': re.compile(r'((Caught signal: |received signal )(SIG[A-Z]+))|:FATAL:'),
    'check_failure': re.compile(r'Check failed:'),
    'gtest_failure_line': re.compile(r'[\w/.-]+:\d+: Failure'),
    'ninja_error': re.compile(r'usage: ninja|unrecognized option|^FAILED: |fatal:'),
    'error': re.compile(r'error:', re.IGNORECASE),
    'warning': re.compile(r'warning:', re.IGNORECASE)
}

# Build log patterns
BUILD_STEP_PATTERN = re.compile(
    r'^(?:\[.*\]\s*)?(CC|CXX|ACTION|LINK|SOLINK|AR|STAMP)\s+(.*)')
FAILED_PATTERN = re.compile(r'^FAILED: ')
NINJA_FATAL_PATTERN = re.compile(r'ninja: fatal:')
GN_WARNING_PATTERN = re.compile(r'WARNING at ')
END_ERROR_PATTERN = re.compile(r'\d+\s+errors? generated\.')
REGENERATING_NINJA_PATTERN = re.compile(r'Regenerating ninja files')
NINJA_ERROR_PATTERN = re.compile(r'ninja: (?:fatal|build stopped:.*)')
BUILD_ERROR_TRIGGER_PATTERN = re.compile(
    r'^(FAILED: |ERROR Unresolved dependencies\.|ninja: error:)|error:')
RETURN_CODE_PATTERN = re.compile(r'RETURN_CODE:')

# Gtest patterns for report generation
GTEST_RUN_PATTERN_SIMPLE = re.compile(r'\[ RUN      \]')
GTEST_OK_PATTERN_SIMPLE = re.compile(r'\[       OK \] ')
GTEST_FAILED_PATTERN_SIMPLE = re.compile(r'\[  FAILED  \]')
GTEST_SKIPPED_PATTERN_SIMPLE = re.compile(r'\[  SKIPPED \]')
