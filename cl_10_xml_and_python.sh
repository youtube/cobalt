#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks \
  tools/metrics/histograms/metadata/cobalt/histograms.xml \
  cobalt/tools/memory_verification_rig/verify_all_cujs.py \
  cobalt/tools/memory_verification_rig/verify_memory_accounting_coverage.py \
  cobalt/tools/memory_verification_rig/verify_memory_accounting_coverage_test.py \
  cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py
