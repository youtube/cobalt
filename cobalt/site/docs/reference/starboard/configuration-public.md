Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Configuration Reference Guide

## Media Configuration

| Properties |
| :--- |
| **`SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES`**<br><br>The implementation is allowed to support kSbMediaAudioSampleTypeInt16 only when this macro is defined.<br><br>By default, this property is undefined. |


## Memory Configuration

| Properties |
| :--- |
| **`SB_CAN_MAP_EXECUTABLE_MEMORY`**<br><br>Whether this platform can map executable memory. Implies SB_HAS_MMAP. This is required for platforms that want to JIT.<br><br>The default value in the Stub implementation is `1` |


## Network Configuration

| Properties |
| :--- |
| **`SB_HAS_IPV6`**<br><br>Specifies whether this platform supports IPV6.<br><br>The default value in the Stub implementation is `1` |


## System Header Configuration

 Any system headers listed here that are not provided by the platform will be emulated in starboard/types.h.

| Properties |
| :--- |
| **`SB_HAS_SYS_TYPES_H`**<br><br> Whether the current platform provides the standard header sys/types.h.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_SSIZE_T`**<br><br>Whether the current platform provides ssize_t.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UTF32`**<br><br>Type detection for wchar_t.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UTF16`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UNSIGNED`**<br><br>Chrome only defines this for ARMEL. Chrome has an exclusion for iOS here, we should too when we support iOS.<br><br>The default value in the Stub implementation is `1` |
