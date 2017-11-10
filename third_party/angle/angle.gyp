# Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'variables':
    {
        'angle_build_winrt%': 1,
        'angle_code': 1,
        'angle_gen_path': '<(SHARED_INTERMEDIATE_DIR)/angle',
        'angle_use_commit_id%': 0,
        'angle_enable_d3d9%': 0,
        'angle_enable_d3d11%': 1,
        'enable_d3d11_feature_level_11%': 0,
        'angle_enable_gl%': 0,
        'angle_use_glx%': 0,
        'angle_enable_vulkan%': 0,
        'angle_enable_essl%': 1, # Enable this for all configs by default
        'angle_enable_glsl%': 1, # Enable this for all configs by default
        'angle_enable_hlsl%': 1,
        'angle_link_glx%': 0,
        'angle_gl_library_type%': 'static_library',
        'dcheck_always_on%': 0,
        'angle_enable_null%': 1, # Available on all platforms

        # On Starboard platforms, the target Angle platform must be explicitly
        # defined in the platform configuration.
        'angle_platform_linux%': 0,
        'angle_platform_posix%': 0,
        'angle_platform_windows%': 0,
    },
    'includes':
    [
        './src/compiler.gypi',
        './src/libGLESv2.gypi',
        './src/libEGL.gypi',
        './src/vulkan_support/vulkan.gypi',
    ],

    'targets':
    [
        {
            'target_name': 'angle_common',
            'type': 'static_library',
            'includes': [ './gyp/common_defines.gypi', ],
            'sources':
            [
                '<@(libangle_common_sources)',
            ],
            'include_dirs':
            [
                './include',
                './src',
                './src/common/third_party/numerics',
            ],
            'direct_dependent_settings':
            {
                'include_dirs':
                [
                    '<(DEPTH)/third_party/angle/include',
                    '<(DEPTH)/third_party/angle/src',
                    '<(DEPTH)/third_party/angle/src/common/third_party/numerics',
                ],
                'conditions':
                [
                    ['dcheck_always_on==1',
                    {
                        'configurations':
                        {
                            'Release_Base':
                            {
                                'defines':
                                [
                                    'ANGLE_ENABLE_RELEASE_ASSERTS',
                                ],
                            },
                        },
                    }],
                    ['target_os=="win"',
                    {
                        'configurations':
                        {
                            'Debug_Base':
                            {
                                'defines':
                                [
                                    'ANGLE_ENABLE_DEBUG_ANNOTATIONS'
                                ],
                            },
                        },
                    }],
                ],
            },
            'conditions':
            [
                ['dcheck_always_on==1',
                {
                    'configurations':
                    {
                        'Release_Base':
                        {
                            'defines':
                            [
                                'ANGLE_ENABLE_RELEASE_ASSERTS',
                            ],
                        },
                    },
                }],
                ['target_os=="win"',
                {
                    'configurations':
                    {
                        'Debug_Base':
                        {
                            'defines':
                            [
                                'ANGLE_ENABLE_DEBUG_ANNOTATIONS'
                            ],
                        },
                    },
                    'sources':
                    [
                        '<@(libangle_common_win_sources)',
                    ],
                }],
                ['OS=="mac"',
                {
                    'sources':
                    [
                        '<@(libangle_common_mac_sources)',
                    ],
                    'link_settings':
                    {
                        'libraries':
                        [
                            '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
                            '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
                        ],
                    },
                }],
                ['OS=="linux"',
                {
                    'sources':
                    [
                        '<@(libangle_common_linux_sources)',
                    ],
                }]
            ],
        },

        {
            'target_name': 'angle_image_util',
            'type': 'static_library',
            'includes': [ './gyp/common_defines.gypi', ],
            'sources':
            [
                '<@(libangle_image_util_sources)',
            ],
            'include_dirs':
            [
                './include',
                './src',
            ],
            'dependencies':
            [
                'angle_common',
            ],
            'direct_dependent_settings':
            {
                'include_dirs':
                [
                    '<(DEPTH)/third_party/angle/include',
                    '<(DEPTH)/third_party/angle/src',
                ],
            },
        },

        {
            'target_name': 'angle_gpu_info_util',
            'type': 'static_library',
            'includes': [ './gyp/common_defines.gypi', ],
            'sources':
            [
                '<@(libangle_gpu_info_util_sources)',
            ],
            'include_dirs':
            [
                './include',
                './src',
            ],
            'dependencies':
            [
                'angle_common',
            ],
            'direct_dependent_settings':
            {
                'include_dirs':
                [
                    '<(DEPTH)/third_party/angle/include',
                    '<(DEPTH)/third_party/angle/src',
                ],
            },
            'conditions':
            [
                ['target_os=="win"',
                {
                    'sources':
                    [
                        '<@(libangle_gpu_info_util_win_sources)',
                    ],
                }],
                ['target_os=="win" and angle_build_winrt==0',
                {
                    'link_settings':
                    {
                        'msvs_settings':
                        {
                            'VCLinkerTool':
                            {
                                'AdditionalDependencies':
                                [
                                    'setupapi.lib'
                                ]
                            }
                        }
                    },
                    'defines':
                    [
                        'GPU_INFO_USE_SETUPAPI',
                    ],
                },
                {
                    'link_settings':
                    {
                        'msvs_settings':
                        {
                            'VCLinkerTool':
                            {
                                'AdditionalDependencies':
                                [
                                    'dxgi.lib'
                                ]
                            }
                        }
                    },
                    'defines':
                    [
                        'GPU_INFO_USE_DXGI',
                    ],
                }],
                ['OS=="linux"',
                {
                    'sources':
                    [
                        '<@(libangle_gpu_info_util_linux_sources)',
                    ],
                }],
                ['OS=="linux" and use_libpci==1',
                {
                    'sources':
                    [
                        '<@(libangle_gpu_info_util_libpci_sources)',
                    ],
                    'defines':
                    [
                        'GPU_INFO_USE_LIBPCI',
                    ],
                    'link_settings':
                    {
                        'ldflags':
                        [
                            '<!@(<(pkg-config) --libs-only-L --libs-only-other libpci)',
                        ],
                        'libraries':
                        [
                            '<!@(<(pkg-config) --libs-only-l libpci)',
                        ],
                    },
                }],
                ['OS=="mac"',
                {
                    'sources':
                    [
                        '<@(libangle_gpu_info_util_mac_sources)',
                    ],
                }],
            ],
        },

        {
            'target_name': 'copy_scripts',
            'type': 'none',
            'includes': [ './gyp/common_defines.gypi', ],
            'hard_dependency': 1,
            'copies':
            [
                {
                    'destination': '<(angle_gen_path)',
                    'files': [ './src/copy_compiler_dll.bat' ],
                },
            ],
            'conditions':
            [
                ['angle_build_winrt==1',
                {
                    'type' : 'shared_library',
                }],
            ],
        },
    ],
    'conditions':
    [
        ['target_os=="win"',
        {
            'targets':
            [
                {
                    'target_name': 'copy_compiler_dll',
                    'type': 'none',
                    'dependencies': [ 'copy_scripts', ],
                    'includes': [ './gyp/common_defines.gypi', ],
                    'conditions':
                    [
                        ['angle_build_winrt==0',
                        {
                            'actions':
                            [
                                {
                                    'action_name': 'copy_dll',
                                    'message': 'Copying D3D Compiler DLL...',
                                    'msvs_cygwin_shell': 0,
                                    'inputs': [ './src/copy_compiler_dll.bat' ],
                                    'outputs': [ '<(PRODUCT_DIR)/d3dcompiler_47.dll' ],
                                    'action':
                                    [
                                        "<(DEPTH)/third_party/angle/src/copy_compiler_dll.bat",
                                        "x64",
                                        "<(windows_sdk_path)",
                                        "<(PRODUCT_DIR)"
                                    ],
                                },
                            ], #actions
                        }],
                        ['angle_build_winrt==1',
                        {
                            'type' : 'shared_library',
                        }],
                    ]
                },
            ], # targets
        }],
    ] # conditions
}
