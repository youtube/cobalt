# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gn_utils


class TranslationContext:

    def __init__(self, import_channel: str):
        self.import_channel = import_channel
        self.module_prefix = f'{import_channel}_cronet_'
        self.include_dirs_denylist = [
            f'external/cronet/{import_channel}/third_party/zlib/',
        ]
        self.cc_defaults_module = f'{self.module_prefix}cc_defaults'
        self.java_framework_defaults_module = f'{self.module_prefix}java_framework_defaults'
        self.tree_path = f'external/cronet/{import_channel}'
        self.additional_args = self._initialize_additional_args()

    def _initialize_additional_args(self):
        prefix = self.module_prefix
        args = {
            # TODO: operating on the final module names means we have to use short
            # names which are less readable. Find a better way.
            f'{prefix}39ea1a33_quiche_net_quic_test_tools_proto_gen_h': [
                ('export_include_dirs', {
                    "net/third_party/quiche/src",
                })
            ],
            f'{prefix}39ea1a33_quiche_net_quic_test_tools_proto_gen__testing_h':
            [('export_include_dirs', {
                "net/third_party/quiche/src",
            })],
            # TODO: fix upstream. Both //base:base and
            # //base/allocator/partition_allocator:partition_alloc do not create a
            # dependency on gtest despite using gtest_prod.h.
            f'{prefix}base_base': [
                ('header_libs', {
                    'libgtest_prod_headers',
                }),
                ('export_header_lib_headers', {
                    'libgtest_prod_headers',
                }),
            ],
            f'{prefix}base_allocator_partition_allocator_partition_alloc': [
                ('header_libs', {
                    'libgtest_prod_headers',
                }),
            ],
            # TODO(b/309920629): Remove once upstreamed.
            f'{prefix}components_cronet_android_cronet_api_java__unfiltered': [
                ('srcs', {
                    'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
                    'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
                }),
            ],
            f'{prefix}components_cronet_android_cronet_api_java__testing__unfiltered':
            [
                ('srcs', {
                    'components/cronet/android/api/src/org/chromium/net/UploadDataProviders.java',
                    'components/cronet/android/api/src/org/chromium/net/apihelpers/UploadDataProviders.java',
                }),
            ],
            f'{prefix}components_cronet_android_cronet_javatests__testing__unfiltered':
            [
                # Needed to @SkipPresubmit annotations
                ('static_libs', {
                    'net-tests-utils-host-device-common',
                }),
            ],
            f'{prefix}components_cronet_android_cronet__testing': [
                ('target', ('android_riscv64', {
                    'stem': "libmainlinecronet_riscv64"
                })),
                ('comment', """TODO: remove stem for riscv64
// This is essential as there can't be two different modules
// with the same output. We usually got away with that because
// the non-testing Cronet is part of the Tethering APEX and the
// testing Cronet is not part of the Tethering APEX which made them
// look like two different outputs from the build system perspective.
// However, Cronet does not ship to Tethering APEX for RISCV64 which
// raises the conflict. Once we start shipping Cronet for RISCV64,
// this can be removed."""),
            ],
            f'{prefix}third_party_netty_tcnative_netty_tcnative_so__testing': [
                ('cflags', {"-Wno-error=pointer-bool-conversion"})
            ],
            f'{prefix}third_party_apache_portable_runtime_apr__testing': [
                ('cflags', {
                    "-Wno-incompatible-pointer-types-discards-qualifiers",
                })
            ],
            # TODO(b/324872305): Remove when gn desc expands public_configs and update code to propagate the
            # include_dir from the public_configs
            # We had to add the export_include_dirs for each target because soong generates each header
            # file in a specific directory named after the target.
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_chromecast_buildflags':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_chromecast_buildflags__testing':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_chromeos_buildflags':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_chromeos_buildflags__testing':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_debugging_buildflags':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_debugging_buildflags__testing':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_buildflags':
            [('export_include_dirs', {
                ".",
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_buildflags__testing':
            [('export_include_dirs', {
                ".",
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_raw_ptr_buildflags':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            f'{prefix}base_allocator_partition_allocator_src_partition_alloc_raw_ptr_buildflags__testing':
            [('export_include_dirs', {
                "base/allocator/partition_allocator/src/",
            })],
            # Protobuf depends on Unsafe class which is used to perform unsafe native methods. This class is not
            # available in the public API provided by the android platform. It's only available by compiling
            # against `core_current` and adding `libcore_private.stubs` as a dependency.
            # defaults have to be removed to prevent sdk_version collision.
            f'{prefix}third_party_protobuf_proto_runtime_lite_java__testing__unfiltered':
            [
                ('libs', {
                    "libcore_private.stubs",
                }),
                ('defaults', None),
                ('sdk_version', 'core_current'),
            ],
            # Protobuf depends on Unsafe class which is used to perform unsafe native methods. This class is not
            # available in the public API provided by the android platform. It's only available by compiling
            # against `core_current` and adding `libcore_private.stubs` as a dependency.
            # defaults have to be removed to prevent sdk_version collision.
            f'{prefix}third_party_protobuf_proto_runtime_lite_java__unfiltered':
            [
                ('libs', {
                    "libcore_private.stubs",
                }),
                ('defaults', None),
                ('sdk_version', 'core_current'),
            ],
            f'{prefix}base_base_java_test_support__testing': [
                ('errorprone', ('javacflags', {
                    "-Xep:ReturnValueIgnored:WARN",
                }))
            ],
            f'{prefix}third_party_perfetto_gn_gen_buildflags': [
                ('export_include_dirs', {
                    "third_party/perfetto/build_config/",
                })
            ],
            f'{prefix}third_party_perfetto_gn_gen_buildflags__testing': [
                ('export_include_dirs', {
                    "third_party/perfetto/build_config/",
                })
            ],
            # See https://crbug.com/517894073#comment5
            f'lib{prefix}third_party_boringssl_raw_bssl_sys_bindings': [
                ('export_include_dirs', {
                    "third_party/boringssl/src/include",
                }),
                ('local_include_dirs', {
                    "third_party/boringssl/src/include",
                })
            ],
            # end export_include_dir.
            # TODO: https://crbug.com/418746360 - Handle //base:build_date_internal
            # for os:linux_glibc.
            f'{prefix}base_build_date_internal__testing': [('host_supported',
                                                            True)],
        }

        args = {
            "{}{}".format(key, suffix): value
            for key, value in args.items()
            for suffix in gn_utils.POSSIBLE_SUFFIXES
        }

        args.update(
            {f'{prefix}components_cronet_android_cronet': [('afdo', True)]})
        return args
