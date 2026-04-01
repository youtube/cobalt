#!/bin/bash

# Exit on error
set -e

DRY_RUN=false
CONFIGURE=true
BUILD=true
RUN_TESTS=true
ANALYZE=true
LOG_DIR=""
TEST_FILTER=""

for arg in "$@"; do
  case $arg in
    --dry-run|-n)
      DRY_RUN=true
      ;;
    --no-configure)
      CONFIGURE=false
      ;;
    --no-build)
      BUILD=false
      ;;
    --no-run)
      RUN_TESTS=false
      ;;
    --no-analyze)
      ANALYZE=false
      ;;
    --analyze=*)
      CONFIGURE=false
      BUILD=false
      RUN_TESTS=false
      ANALYZE=true
      LOG_DIR="${arg#*=}"
      ;;
    --tests=*)
      TEST_FILTER="${arg#*=}"
      ;;
  esac
done

# --- Parse test filter ---
INCLUDES=()
EXCLUDES=()
if [ -n "$TEST_FILTER" ]; then
  IFS=':' read -r -a FILTER_ARRAY <<< "$TEST_FILTER"
  for f in "${FILTER_ARRAY[@]}"; do
    if [[ $f == -* ]]; then
      EXCLUDES+=("${f#-}")
    else
      INCLUDES+=("$f")
    fi
  done
fi

PLATFORM="linux-x64x11"
BUILD_TYPE="devel"
OUT_DIR="out/${PLATFORM}_${BUILD_TYPE}"
TEST_TARGETS_FILE="cobalt/build/testing/targets/${PLATFORM}/test_targets.json"

# --- 1. Configure with GN ---
if [ "$CONFIGURE" = true ]; then
  echo "🔧 Configuring with gn.py..."
  python3 cobalt/build/gn.py -p ${PLATFORM} -c ${BUILD_TYPE} --asan --lsan

  if [ "$DRY_RUN" = true ]; then
    echo "--- args.gn content ---"
    if [ -f "${OUT_DIR}/args.gn" ]; then
      cat "${OUT_DIR}/args.gn"
    else
      echo "args.gn not found in ${OUT_DIR}"
    fi
    echo "-----------------------"
  fi
else
  echo "⏭️ Skipping configuration."
fi

# --- 2. Extract test targets ---
echo "📋 Extracting test targets from ${TEST_TARGETS_FILE}..."
# We use the 'test_targets' field from the JSON to get the GN targets to build.
# We skip tests with "perf" in the name.
if command -v jq &> /dev/null; then
  TARGETS=$(jq -r '.test_targets[]' "${TEST_TARGETS_FILE}" | grep -v "perf")
else
  # Fallback: extract strings that look like targets (containing :)
  TARGETS=$(grep -o '"[^"]*:[^"]*"' "${TEST_TARGETS_FILE}" | tr -d '"' | grep -v "TODO" | grep -v "perf" || true)
fi

if [ -z "$TARGETS" ]; then
  echo "❌ Error: No test targets found."
  exit 1
fi

# Apply test filter if provided
if [ -n "$TEST_FILTER" ]; then
  FILTERED_TARGETS=""
  for T in $TARGETS; do
    T_NAME="${T##*:}"
    
    # Check for positive inclusion
    FOUND=false
    for INC in "${INCLUDES[@]}"; do
      if [ "$T_NAME" == "$INC" ]; then
        FOUND=true
        break
      fi
    done
    [ "$FOUND" = false ] && continue

    # Check for exclusion
    SKIP=false
    for EX in "${EXCLUDES[@]}"; do
      if [ "$T_NAME" == "$EX" ]; then
        SKIP=true
        break
      fi
    done
    [ "$SKIP" = true ] && continue
    
    FILTERED_TARGETS="${FILTERED_TARGETS}${T}"$'\n'
  done
  TARGETS=$(echo "$FILTERED_TARGETS" | sed '/^$/d')

  if [ -z "$TARGETS" ]; then
    echo "❌ Error: No test targets match the filter: ${TEST_FILTER}"
    exit 1
  fi
fi

if [ "$DRY_RUN" = true ]; then
  echo "--- Test Targets ---"
  echo "$TARGETS"
  echo "--------------------"
fi

# --- 3. Build test targets ---
if [ "$BUILD" = true ]; then
  echo "🔨 Building all test targets..."
  # Convert newlines to spaces for autoninja
  TARGETS_SPACE_SEPARATED=$(echo "$TARGETS" | tr '\n' ' ')
  autoninja -C "${OUT_DIR}" ${TARGETS_SPACE_SEPARATED}
else
  echo "⏭️ Skipping build."
fi

# --- 4. Run tests and log output ---
if [ "$RUN_TESTS" = true ]; then
  # Use a provided log directory, or create a temporary one.
  if [ -z "$LOG_DIR" ]; then
    if [ -n "$COBALT_LOG_DIR" ]; then
      LOG_DIR="$COBALT_LOG_DIR"
    else
      LOG_DIR=$(mktemp -d -t cobalt_test_logs_XXXXXX)
    fi
  fi
  mkdir -p "$LOG_DIR"
  echo "🚀 Running tests in Docker. Logs will be in: ${LOG_DIR}"

  # --- Environment Setup (Matching CI) ---
  # These will be passed to the Docker container
  ASAN_OPTS="allocator_may_return_null=1 symbolize=1 external_symbolizer_path=./third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer detect_leaks=1 detect_odr_violation=0"
  
  ABS_OUT_DIR="$(pwd)/${OUT_DIR}"
  LD_LIB_PATH="${ABS_OUT_DIR}/starboard:${ABS_OUT_DIR}:${LD_LIBRARY_PATH}"

  # We use the 'executables' field to know what to run. 
  # We skip tests with "perf" in the name.
  if command -v jq &> /dev/null; then
    EXECUTABLES=$(jq -r '.executables[]' "${TEST_TARGETS_FILE}" | grep -v "perf")
  else
    # Fallback: extract executable paths.
    EXECUTABLES=$(grep -o '"out/linux-x64x11_devel/[^"]*"' "${TEST_TARGETS_FILE}" | tr -d '"' | grep -v "perf")
  fi

  # Apply test filter if provided
  if [ -n "$TEST_FILTER" ]; then
    FILTERED_EXECUTABLES=""
    for E in $EXECUTABLES; do
      E_NAME="${E##*/}"
      
      # Check for positive inclusion
      FOUND=false
      for INC in "${INCLUDES[@]}"; do
        if [ "$E_NAME" == "$INC" ]; then
          FOUND=true
          break
        fi
      done
      [ "$FOUND" = false ] && continue

      # Check for exclusion
      SKIP=false
      for EX in "${EXCLUDES[@]}"; do
        if [ "$E_NAME" == "$EX" ]; then
          SKIP=true
          break
        fi
      done
      [ "$SKIP" = true ] && continue
      
      FILTERED_EXECUTABLES="${FILTERED_EXECUTABLES}${E}"$'\n'
    done
    EXECUTABLES=$(echo "$FILTERED_EXECUTABLES" | sed '/^$/d')
  fi

  # For each executable, we need to map from the placeholder path in the JSON 
  # to the actual path in our build directory.
  for EXE_PLACEHOLDER in $EXECUTABLES; do
    EXE_NAME=$(basename "$EXE_PLACEHOLDER")
    # Use the actual OUT_DIR we configured earlier
    EXE_PATH="${OUT_DIR}/${EXE_NAME}"
    LOG_FILE="${LOG_DIR}/${EXE_NAME}.log"
    
    # --- Filter Logic ---
    FILTER_FILE="cobalt/testing/filters/${PLATFORM}/${EXE_NAME}_filter.json"
    GTEST_FILTER_FLAG=""
    
    if [ -f "$FILTER_FILE" ]; then
      if command -v jq &> /dev/null; then
        FAILING_TESTS=$(jq -r '.failing_tests | join(":")' "$FILTER_FILE")
        if [ -n "$FAILING_TESTS" ] && [ "$FAILING_TESTS" != "null" ]; then
          GTEST_FILTER_FLAG="--gtest_filter=-${FAILING_TESTS}"
        fi
      else
        # Simple fallback: extract all strings in the failing_tests array
        FAILING_TESTS=$(grep -o '"[^"]*"' "$FILTER_FILE" | tr -d '"' | tr '\n' ':' | sed 's/:$//' || true)
        if [ -n "$FAILING_TESTS" ]; then
          GTEST_FILTER_FLAG="--gtest_filter=-${FAILING_TESTS}"
        fi
      fi
    fi

    echo -n "🏃 Running ${EXE_NAME}... "
    [ -n "$GTEST_FILTER_FLAG" ] && echo -n "(with filters) "
    
    # Check if we should skip the entire suite because of a '*' filter
    if [ "$FAILING_TESTS" == "*" ]; then
      echo "⏭️ SKIPPED (Filter is '*')"
      if [ "$DRY_RUN" = false ]; then
        echo "Skipped due to test filter." > "${LOG_FILE}"
      fi
      continue
    fi

    # Check if executable exists (it should if build succeeded)
    if [ -f "$EXE_PATH" ]; then
      # Construct the command to run inside the container
      # Note: Using :99 and 1024x768x24 as in the user's example
      CONTAINER_CMD="Xvfb :99 -screen 0 1024x768x24 & export DISPLAY=:99 && ./${EXE_PATH} --single-process-tests ${GTEST_FILTER_FLAG}"

      # Construct the full Docker command
      DOCKER_CMD="docker run --rm \
        -v $(pwd):$(pwd) \
        -w $(pwd) \
        -e ASAN_OPTIONS=\"${ASAN_OPTS}\" \
        -e LD_LIBRARY_PATH=\"${LD_LIB_PATH}\" \
        ghcr.io/youtube/cobalt/unittest:main \
        sh -c \"${CONTAINER_CMD}\""

      if [ "$DRY_RUN" = true ]; then
        echo "DRY-RUN: ${DOCKER_CMD} > ${LOG_FILE} 2>&1"
      else
        set +e
        # Use eval to handle the nested quoting correctly
        eval "${DOCKER_CMD}" > "${LOG_FILE}" 2>&1
        RESULT=$?
        set -e
        
        if [ $RESULT -eq 0 ]; then
          echo "✅ PASSED"
        else
          echo "❌ FAILED (Exit code: $RESULT). Log: ${LOG_FILE}"
        fi
      fi
    else
      echo "⚠️ NOT FOUND: ${EXE_PATH}"
    fi
  done
else
  echo "⏭️ Skipping running tests."
  if [ -z "$LOG_DIR" ]; then
    LOG_DIR="$COBALT_LOG_DIR"
  fi
fi

# --- 5. Analyze logs ---
if [ "$ANALYZE" = true ] && [ "$DRY_RUN" = false ]; then
  if [ -n "$LOG_DIR" ] && [ -d "$LOG_DIR" ]; then
    echo ""
    echo "🔍 Analyzing logs for ASAN/LSAN failures in ${LOG_DIR}..."
    
    ASAN_LSAN_FAILURES=""
    for LOG_FILE in "${LOG_DIR}"/*.log; do
      if [ -f "$LOG_FILE" ]; then
        TEST_NAME=$(basename "$LOG_FILE" .log)
        # Search for common ASAN/LSAN failure patterns
        if grep -qiE "AddressSanitizer|LeakSanitizer|detected memory leaks|SUMMARY: AddressSanitizer:" "$LOG_FILE"; then
          ASAN_LSAN_FAILURES="${ASAN_LSAN_FAILURES}${TEST_NAME}\n"
        fi
      fi
    done

    if [ -n "$ASAN_LSAN_FAILURES" ]; then
      echo "🚨 Tests with ASAN/LSAN failures:"
      echo -e "$ASAN_LSAN_FAILURES"
    else
      echo "✅ No ASAN/LSAN failures detected in logs."
    fi
  else
    if [ "$RUN_TESTS" = false ]; then
       echo "⚠️ Skipping analysis: Log directory not found or not provided."
    fi
  fi
fi

echo ""
echo "✅ All steps completed."
if [ -n "$LOG_DIR" ]; then
  echo "Logs are available in: ${LOG_DIR}"
fi
