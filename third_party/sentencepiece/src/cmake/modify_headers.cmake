# CMake script to modify headers for installation
# Replaces #include "absl/..." with #include <absl/...>

if (NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE must be defined")
endif()

file(READ "${INPUT_FILE}" content)
# Replace #include "absl/..."" with #include <absl/...>
string(REGEX REPLACE "#include \"third_party/(absl/[^\"]+)\"" "#include <\\1>" content "${content}")
file(WRITE "${OUTPUT_FILE}" "${content}")
