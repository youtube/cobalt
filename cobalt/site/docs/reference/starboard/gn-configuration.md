Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard: configuration.gni Reference Guide

| Variables |
| :--- |
| **`cobalt_platform_dependencies`**<br><br> List of platform-specific targets that get compiled into cobalt.<br><br>The default value is `[]`. |
| **`final_executable_type`**<br><br> The target type for executable targets. Allows changing the target type on platforms where the native code may require an additional packaging step (ex. Android).<br><br>The default value is `"shared_library"`. |
| **`gl_type`**<br><br> The source of EGL and GLES headers and libraries. Valid values (case and everything sensitive!):<ul><li><code>system_gles2</code> - Use the system implementation of EGL + GLES2. The headers and libraries must be on the system include and link paths.<li><code>angle</code> - Use ANGLE's EGL + GLES2 implementation. This requires a valid ANGLE implementation for the platform.<br><br>The default value is `"system_gles2"`. |
| **`has_platform_targets`**<br><br> Whether the platform has platform-specific targets to depend on.<br><br>The default value is `false`. |
| **`platform_tests_path`**<br><br> Set to the starboard_platform_tests target if the platform implements them.<br><br>The default value is `""`. |
| **`sabi_path`**<br><br> Where the Starboard ABI file for this platform can be found.<br><br>The default value is `"starboard/sabi/default/sabi.json"`. |
| **`sb_api_version`**<br><br> The Starboard API version of the current build configuration. The default value is meant to be overridden by a Starboard ABI file.<br><br>The default value is `18`. |
| **`sb_enable_benchmark`**<br><br> Used to enable benchmarks.<br><br>The default value is `false`. |
| **`sb_enable_cpp17_audit`**<br><br> Enables an NPLB audit of C++17 support.<br><br>The default value is `true`. |
| **`sb_enable_cpp20_audit`**<br><br> Enables an NPLB audit of C++20 support.<br><br>The default value is `true`. |
| **`sb_enable_lib`**<br><br> Enables embedding Cobalt as a shared library within another app. This requires a 'lib' starboard implementation for the corresponding platform.<br><br>The default value is `false`. |
| **`sb_evergreen_compatible_use_libunwind`**<br><br> Whether to use the libunwind library on Evergreen compatible platform.<br><br>The default value is `false`. |
| **`sb_filter_based_player`**<br><br> Used to indicate that the player is filter based.<br><br>The default value is `true`. |
| **`sb_static_contents_output_data_dir`**<br><br> Directory path to static contents' data.<br><br>The default value is `"/project_out_dir"`. |
| **`source_set_configs`**<br><br> Target-specific configurations for source_set targets.<br><br>The default value is `[]`. |
| **`starboard_level_final_executable_type`**<br><br>The default value is `"executable"`. |
| **`starboard_level_gtest_target_type`**<br><br>The default value is `"executable"`. |
| **`static_library_configs`**<br><br> Target-specific configurations for static_library targets.<br><br>The default value is `[]`. |
