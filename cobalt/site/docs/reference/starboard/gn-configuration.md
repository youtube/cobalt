---
layout: doc
title: "Starboard: configuration.gni Reference Guide"
---

| Variables |
| :--- |
| **`abort_on_allocation_failure`**<br><br> Halt execution on failure to allocate memory.<br><br>The default value is `true`. |
| **`asan_symbolizer_path`**<br><br> A symbolizer path for ASAN can be added to allow translation of callstacks.<br><br>The default value is `"<clang_base_path>/bin/llvm-symbolizer"`. |
| **`cobalt_licenses_platform`**<br><br> Sub-directory to copy license file to.<br><br>The default value is `"default"`. |
| **`cobalt_platform_dependencies`**<br><br> List of platform-specific targets that get compiled into cobalt.<br><br>The default value is `[]`. |
| **`cobalt_v8_emit_builtins_as_inline_asm`**<br><br> Some compiler can not compile with raw assembly(.S files) and v8 converts asm to inline assembly for these platforms.<br><br>The default value is `false`. |
| **`default_renderer_options_dependency`**<br><br> Override this value to adjust the default rasterizer setting for your platform.<br><br>The default value is `"//cobalt/renderer:default_options"`. |
| **`enable_account_manager`**<br><br> Set to true to enable H5vccAccountManager.<br><br>The default value is `false`. |
| **`enable_in_app_dial`**<br><br> Enables or disables the DIAL server that runs inside Cobalt. Note: Only enable if there's no system-wide DIAL support.<br><br>The default value is `false`. |
| **`executable_configs`**<br><br> Target-specific configurations for executable targets.<br><br>The default value is `[]`. |
| **`final_executable_type`**<br><br> The target type for executable targets. Allows changing the target type on platforms where the native code may require an additional packaging step (ex. Android).<br><br>The default value is `"executable"`. |
| **`gl_type`**<br><br> The source of EGL and GLES headers and libraries. Valid values (case and everything sensitive!):<ul><li><code>none</code> - No EGL + GLES implementation is available on this platform.<li><code>system_gles2</code> - Use the system implementation of EGL + GLES2. The headers and libraries must be on the system include and link paths.<li><code>glimp</code> - Cobalt's own EGL + GLES2 implementation. This requires a valid Glimp implementation for the platform.<li><code>angle</code> - A DirectX-to-OpenGL adaptation layer. This requires a valid ANGLE implementation for the platform.<br><br>The default value is `"system_gles2"`. |
| **`gtest_target_type`**<br><br> The target type for test targets. Allows changing the target type on platforms where the native code may require an additional packaging step (ex. Android).<br><br>The default value is `"executable"`. |
| **`has_platform_targets`**<br><br> Whether the platform has platform-specific targets to depend on.<br><br>The default value is `false`. |
| **`install_target_path`**<br><br> The path to the gni file containing the install_target template which defines how the build should produce the install/ directory.<br><br>The default value is `"//starboard/build/install/no_install.gni"`. |
| **`loadable_module_configs`**<br><br> Target-specific configurations for loadable_module targets.<br><br>The default value is `[]`. |
| **`path_to_yasm`**<br><br> Where yasm can be found on the host device.<br><br>The default value is `"yasm"`. |
| **`platform_tests_path`**<br><br> Set to the starboard_platform_tests target if the platform implements them.<br><br>The default value is `""`. |
| **`sabi_path`**<br><br> Where the Starboard ABI file for this platform can be found.<br><br>The default value is `"starboard/sabi/default/sabi.json"`. |
| **`sb_api_version`**<br><br> The Starboard API version of the current build configuration. The default value is meant to be overridden by a Starboard ABI file.<br><br>The default value is `15`. |
| **`sb_enable_benchmark`**<br><br> Used to enable benchmarks.<br><br>The default value is `false`. |
| **`sb_enable_cpp17_audit`**<br><br> Enables an NPLB audit of C++17 support.<br><br>The default value is `true`. |
| **`sb_enable_lib`**<br><br> Enables embedding Cobalt as a shared library within another app. This requires a 'lib' starboard implementation for the corresponding platform.<br><br>The default value is `false`. |
| **`sb_enable_opus_sse`**<br><br> Enables optimizations on SSE compatible platforms.<br><br>The default value is `true`. |
| **`sb_evergreen_compatible_enable_lite`**<br><br> Whether to adopt Evergreen Lite on the Evergreen compatible platform.<br><br>The default value is `false`. |
| **`sb_evergreen_compatible_package`**<br><br> Whether to generate the whole package containing both Loader app and Cobalt core on the Evergreen compatible platform.<br><br>The default value is `false`. |
| **`sb_evergreen_compatible_use_libunwind`**<br><br> Whether to use the libunwind library on Evergreen compatible platform.<br><br>The default value is `false`. |
| **`sb_filter_based_player`**<br><br> Used to indicate that the player is filter based.<br><br>The default value is `true`. |
| **`sb_is_evergreen`**<br><br> Whether this is an Evergreen build.<br><br>The default value is `false`. |
| **`sb_is_evergreen_compatible`**<br><br> Whether this is an Evergreen compatible platform. A compatible platform can run the elf_loader and launch the Evergreen build.<br><br>The default value is `false`. |
| **`sb_libevent_method`**<br><br> The event polling mechanism available on this platform to support libevent. Platforms may redefine to 'poll' if necessary. Other mechanisms, e.g. devpoll, kqueue, select, are not yet supported.<br><br>The default value is `"epoll"`. |
| **`sb_static_contents_output_data_dir`**<br><br> Directory path to static contents' data.<br><br>The default value is `"/project_out_dir/content/data"`. |
| **`sb_use_no_rtti`**<br><br> Whether or not to disable run-time type information (adding no_rtti flag).<br><br>The default value is `false`. |
| **`separate_install_targets_for_bundling`**<br><br> Set to true to separate install target directories.<br><br>The default value is `false`. |
| **`shared_library_configs`**<br><br> Target-specific configurations for shared_library targets.<br><br>The default value is `[]`. |
| **`source_set_configs`**<br><br> Target-specific configurations for source_set targets.<br><br>The default value is `[]`. |
| **`static_library_configs`**<br><br> Target-specific configurations for static_library targets.<br><br>The default value is `[]`. |
| **`use_skia_next`**<br><br> Flag to use a future version of Skia, currently not available.<br><br>The default value is `false`. |
| **`use_thin_archive`**<br><br> Whether or not to link with thin archives.<br><br>The default value is `true`. |
| **`v8_enable_pointer_compression_override`**<br><br> Set to true to enable pointer compression for v8.<br><br>The default value is `true`. |
| **`yasm_exists`**<br><br> Enables the yasm compiler to be used to compile .asm files.<br><br>The default value is `false`. |
