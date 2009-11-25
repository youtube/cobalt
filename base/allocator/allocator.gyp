# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'jemalloc_dir': '../../third_party/jemalloc/chromium',
    'tcmalloc_dir': '../../third_party/tcmalloc/chromium',
  },
  'targets': [
    {
      'target_name': 'allocator',
      'type': '<(library)',
      'msvs_guid': 'C564F145-9172-42C3-BFCB-60FDEA124321',
      'include_dirs': [
        '.',
        '<(tcmalloc_dir)/src/base',
        '<(tcmalloc_dir)/src',
        '../..',
      ],
      'defines': [
        'NO_TCMALLOC_SAMPLES',
      ],
      'direct_dependent_settings': {
        'configurations': {
          'Common': {
            'msvs_settings': {
              'VCLinkerTool': {
                'IgnoreDefaultLibraryNames': ['libcmtd.lib', 'libcmt.lib'],
                'AdditionalDependencies': [
                  '<(SHARED_INTERMEDIATE_DIR)/allocator/libcmt.lib'
                ],
              },
            },
          },
        },
        'conditions': [
          ['OS=="win"', {
            'defines': [
              ['PERFTOOLS_DLL_DECL', '']
            ],
          }],
        ],
      },
      'sources': [
        # Generated for our configuration from tcmalloc's build
        # and checked in.
        '<(tcmalloc_dir)/src/config.h',
        '<(tcmalloc_dir)/src/config_linux.h',
        '<(tcmalloc_dir)/src/config_win.h',

        # tcmalloc files
        '<(tcmalloc_dir)/src/base/dynamic_annotations.cc',
        '<(tcmalloc_dir)/src/base/dynamic_annotations.h',
        '<(tcmalloc_dir)/src/base/logging.cc',
        '<(tcmalloc_dir)/src/base/logging.h',
        '<(tcmalloc_dir)/src/base/low_level_alloc.cc',
        '<(tcmalloc_dir)/src/base/low_level_alloc.h',
        '<(tcmalloc_dir)/src/base/spinlock.cc',
        '<(tcmalloc_dir)/src/base/spinlock.h',
        '<(tcmalloc_dir)/src/base/sysinfo.cc',
        '<(tcmalloc_dir)/src/base/sysinfo.h',
        '<(tcmalloc_dir)/src/central_freelist.cc',
        '<(tcmalloc_dir)/src/central_freelist.h',
        '<(tcmalloc_dir)/src/common.cc',
        '<(tcmalloc_dir)/src/common.h',
        '<(tcmalloc_dir)/src/heap-profile-table.cc',
        '<(tcmalloc_dir)/src/heap-profile-table.h',
        '<(tcmalloc_dir)/src/internal_logging.cc',
        '<(tcmalloc_dir)/src/internal_logging.h',
        '<(tcmalloc_dir)/src/linked_list.h',
        '<(tcmalloc_dir)/src/malloc_hook.cc',
        '<(tcmalloc_dir)/src/malloc_hook-inl.h',
        '<(tcmalloc_dir)/src/malloc_extension.cc',
        '<(tcmalloc_dir)/src/google/malloc_extension.h',
        '<(tcmalloc_dir)/src/page_heap.cc',
        '<(tcmalloc_dir)/src/page_heap.h',
        '<(tcmalloc_dir)/src/sampler.cc',
        '<(tcmalloc_dir)/src/sampler.h',
        '<(tcmalloc_dir)/src/span.cc',
        '<(tcmalloc_dir)/src/span.h',
        '<(tcmalloc_dir)/src/stack_trace_table.cc',
        '<(tcmalloc_dir)/src/stack_trace_table.h',
        '<(tcmalloc_dir)/src/stacktrace.cc',
        '<(tcmalloc_dir)/src/stacktrace.h',
        '<(tcmalloc_dir)/src/static_vars.cc',
        '<(tcmalloc_dir)/src/static_vars.h',
        '<(tcmalloc_dir)/src/thread_cache.cc',
        '<(tcmalloc_dir)/src/thread_cache.h',
        '<(tcmalloc_dir)/src/windows/port.cc',
        '<(tcmalloc_dir)/src/windows/port.h',

        # non-windows
        '<(tcmalloc_dir)/src/base/linuxthreads.cc',
        '<(tcmalloc_dir)/src/base/linuxthreads.h',
        '<(tcmalloc_dir)/src/base/vdso_support.cc',
        '<(tcmalloc_dir)/src/base/vdso_support.h',
        '<(tcmalloc_dir)/src/google/tcmalloc.h',
        '<(tcmalloc_dir)/src/maybe_threads.cc',
        '<(tcmalloc_dir)/src/maybe_threads.h',
        '<(tcmalloc_dir)/src/symbolize.cc',
        '<(tcmalloc_dir)/src/symbolize.h',
        '<(tcmalloc_dir)/src/system-alloc.cc',
        '<(tcmalloc_dir)/src/system-alloc.h',
        '<(tcmalloc_dir)/src/tcmalloc.cc',
        '<(tcmalloc_dir)/src/tcmalloc_linux.cc',

        # heap-profiler/checker/cpuprofiler
        '<(tcmalloc_dir)/src/base/thread_lister.c',
        '<(tcmalloc_dir)/src/base/thread_lister.h',
        '<(tcmalloc_dir)/src/heap-checker-bcad.cc',
        '<(tcmalloc_dir)/src/heap-checker.cc',
        '<(tcmalloc_dir)/src/heap-profiler.cc',
        '<(tcmalloc_dir)/src/memory_region_map.cc',
        '<(tcmalloc_dir)/src/memory_region_map.h',
        '<(tcmalloc_dir)/src/profiledata.cc',
        '<(tcmalloc_dir)/src/profiledata.h',
        '<(tcmalloc_dir)/src/profile-handler.cc',
        '<(tcmalloc_dir)/src/profile-handler.h',
        '<(tcmalloc_dir)/src/profiler.cc',
        '<(tcmalloc_dir)/src/raw_printer.cc',
        '<(tcmalloc_dir)/src/raw_printer.h',

        # jemalloc files
        '<(jemalloc_dir)/jemalloc.c',
        '<(jemalloc_dir)/jemalloc.h',
        '<(jemalloc_dir)/ql.h',
        '<(jemalloc_dir)/qr.h',
        '<(jemalloc_dir)/rb.h',

        'allocator_shim.cc',
        'generic_allocators.cc',
        'win_allocator.cc',        
      ],
      # sources! means that these are not compiled directly.
      'sources!': [
        'generic_allocators.cc',
        'win_allocator.cc',

        '<(tcmalloc_dir)/src/tcmalloc.cc',
      ],
      'msvs_settings': {
        # TODO(sgk):  merge this with build/common.gypi settings
        'VCLibrarianTool=': {
          'AdditionalOptions': '/ignore:4006,4221',
          'AdditionalLibraryDirectories':
            ['<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib'],
        },
        'VCLinkerTool': {
          'AdditionalOptions': '/ignore:4006',
        },
      },
      'configurations': {
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'RuntimeLibrary': '0',
            },
          },
        },
      },
      'conditions': [
        ['OS=="win"', {
          'defines': [
            ['PERFTOOLS_DLL_DECL', '']
          ],
          'dependencies': [
            'libcmt',
          ],
          'include_dirs': [
            '<(tcmalloc_dir)/src/windows',
          ],
          'sources!': [
            '<(tcmalloc_dir)/src/base/linuxthreads.cc',
            '<(tcmalloc_dir)/src/base/linuxthreads.h',
            '<(tcmalloc_dir)/src/base/vdso_support.cc',
            '<(tcmalloc_dir)/src/base/vdso_support.h',
            '<(tcmalloc_dir)/src/maybe_threads.cc',
            '<(tcmalloc_dir)/src/maybe_threads.h',
            '<(tcmalloc_dir)/src/symbolize.cc',
            '<(tcmalloc_dir)/src/symbolize.h',
            '<(tcmalloc_dir)/src/system-alloc.cc',
            '<(tcmalloc_dir)/src/system-alloc.h',

            # don't use linux forked version
            '<(tcmalloc_dir)/src/tcmalloc_linux.cc',

            # heap-profiler/checker/cpuprofiler
            '<(tcmalloc_dir)/src/base/thread_lister.c',
            '<(tcmalloc_dir)/src/base/thread_lister.h',
            '<(tcmalloc_dir)/src/heap-checker-bcad.cc',
            '<(tcmalloc_dir)/src/heap-checker.cc',
            '<(tcmalloc_dir)/src/heap-profiler.cc',
            '<(tcmalloc_dir)/src/memory_region_map.cc',
            '<(tcmalloc_dir)/src/memory_region_map.h',
            '<(tcmalloc_dir)/src/profiledata.cc',
            '<(tcmalloc_dir)/src/profiledata.h',
            '<(tcmalloc_dir)/src/profile-handler.cc',
            '<(tcmalloc_dir)/src/profile-handler.h',
            '<(tcmalloc_dir)/src/profiler.cc',
          ],
        }],
        ['OS=="linux"', {
          'sources!': [
            '<(tcmalloc_dir)/src/page_heap.cc',
            '<(tcmalloc_dir)/src/system-alloc.h',
            '<(tcmalloc_dir)/src/windows/port.cc',
            '<(tcmalloc_dir)/src/windows/port.h',

            # TODO(willchan): Support allocator shim later on.
            'allocator_shim.cc',

            # TODO(willchan): support jemalloc on other platforms
            # jemalloc files
            '<(jemalloc_dir)/jemalloc.c',
            '<(jemalloc_dir)/jemalloc.h',
            '<(jemalloc_dir)/ql.h',
            '<(jemalloc_dir)/qr.h',
            '<(jemalloc_dir)/rb.h',
          ],
          'cflags!': [
            '-fvisibility=hidden',
          ],
          'link_settings': {
            'ldflags': [
              # Don't let linker rip this symbol out, otherwise the heap&cpu
              # profilers will not initialize properly on startup.
              '-Wl,-uIsHeapProfilerRunning,-uProfilerStart',
              # Do the same for heap leak checker.
              '-Wl,-u_Z21InitialMallocHook_NewPKvj,-u_Z22InitialMallocHook_MMapPKvS0_jiiix,-u_Z22InitialMallocHook_SbrkPKvi',
              '-Wl,-u_Z21InitialMallocHook_NewPKvm,-u_Z22InitialMallocHook_MMapPKvS0_miiil,-u_Z22InitialMallocHook_SbrkPKvl',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'allocator_unittests',
      'type': 'executable',
      'dependencies': [
        'allocator',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '.',
        '<(tcmalloc_dir)/src/base',
        '<(tcmalloc_dir)/src',
        '../..',
      ],
      'msvs_guid': 'E99DA267-BE90-4F45-1294-6919DB2C9999',
      'sources': [
        'unittest_utils.cc',
        'allocator_unittests.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'libcmt',
          'type': 'none',
          'actions': [
            {
              'action_name': 'libcmt',
              'inputs': [
                'prep_libc.sh',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/allocator/libcmt.lib',
              ],
              'action': [
                './prep_libc.sh',
                '$(VCInstallDir)lib',
                '<(SHARED_INTERMEDIATE_DIR)/allocator',
              ],
            },
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
