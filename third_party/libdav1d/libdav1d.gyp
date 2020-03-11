{
  'variables': {
    'libdav1d_dir': '<(DEPTH)/third_party/libdav1d',

    'libdav1d_include_dirs': [
        '<(libdav1d_dir)/',
        '<(libdav1d_dir)/include',
        '<(libdav1d_dir)/include/dav1d',
    ],

    'libdav1d_bitdepth_sources': [
      'src/cdef.h',
      'src/cdef_apply.h',
      'src/cdef_apply_tmpl.c',
      'src/cdef_tmpl.c',
      'src/fg_apply.h',
      'src/fg_apply_tmpl.c',
      'src/film_grain.h',
      'src/film_grain_tmpl.c',
      'src/ipred.h',
      'src/ipred_prepare.h',
      'src/ipred_prepare_tmpl.c',
      'src/ipred_tmpl.c',
      'src/itx.h',
      'src/itx_tmpl.c',
      'src/lf_apply.h',
      'src/lf_apply_tmpl.c',
      'src/loopfilter.h',
      'src/loopfilter_tmpl.c',
      'src/looprestoration.h',
      'src/looprestoration_tmpl.c',
      'src/lr_apply.h',
      'src/lr_apply_tmpl.c',
      'src/mc.h',
      'src/mc_tmpl.c',
      'src/recon.h',
      'src/recon_tmpl.c',
    ],
  },

  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)/third_party/libdav1d/',
      '<(DEPTH)/third_party/libdav1d/include',
      '<(DEPTH)/third_party/libdav1d/include/dav1d',
    ],
    # These values are determined by the configure script in the project,
    # and included via the |build/config.h| file however in this case we
    # determine these using gyp and inject them into the compilation.
    'defines': [
      'ARCH_AARCH64=SB_IS(ARCH_ARM64)',
      'ARCH_ARM=SB_IS(ARCH_ARM)',
      'ARCH_X86=(SB_IS(ARCH_X86) || SB_IS(ARCH_X64))',
      'ARCH_X86_32=SB_IS(ARCH_X86)',
      'ARCH_X86_64=SB_IS(ARCH_X64)',
      'CONFIG_16BPC=1',
      'CONFIG_8BPC=1',
      'CONFIG_LOG=1',
      'ENDIANNESS_BIG=SB_IS(BIG_ENDIAN)',
      'HAVE_ASM=0',
      'HAVE_CLOCK_GETTIME=1',
      'HAVE_POSIX_MEMALIGN=1',
      'HAVE_UNISTD_H=1',
      'STACK_ALIGNMENT=32'
    ],
  },

  'targets': [
    {
      'target_name': 'libdav1d_entrypoint',
      'type': 'static_library',
      'sources': [
        'src/lib.c',
        'src/thread_task.c',
        'src/thread_task.h',
      ],
    },
    {
      'target_name': 'libdav1d_bitdepth16',
      'type': 'static_library',
      'defines': [
        'BITDEPTH=16',
      ],
      'sources': [
        '<@(libdav1d_bitdepth_sources)',
      ]
    },
    {
      'target_name': 'libdav1d_bitdepth8',
      'type': 'static_library',
      'defines': [
        'BITDEPTH=8',
      ],
      'sources': [
        '<@(libdav1d_bitdepth_sources)',
      ]
    },
    {
      'target_name': 'libdav1d_no_asm',
      'type': 'static_library',
      'sources': [
        'src/cpu.c',
        'src/cpu.h',
        'src/msac.c',
        'src/msac.h',
      ]
    },
    {
      'target_name': 'libdav1d',
      'type': 'static_library',
      'sources': [
        'src/cdf.c',
        'src/cdf.h',
        'src/ctx.h',
        'src/data.c',
        'src/data.h',
        'src/decode.c',
        'src/decode.h',
        'src/dequant_tables.c',
        'src/dequant_tables.h',
        'src/env.h',
        'src/getbits.c',
        'src/getbits.h',
        'src/internal.h',
        'src/intra_edge.c',
        'src/intra_edge.h',
        'src/levels.h',
        'src/lf_mask.c',
        'src/lf_mask.h',
        'src/log.c',
        'src/log.h',
        'src/obu.c',
        'src/obu.h',
        'src/picture.c',
        'src/picture.h',
        'src/qm.c',
        'src/qm.h',
        'src/ref.c',
        'src/ref.h',
        'src/ref_mvs.c',
        'src/ref_mvs.h',
        'src/scan.c',
        'src/scan.h',
        'src/tables.c',
        'src/tables.h',
        'src/thread.h',
        'src/thread_data.h',
        'src/warpmv.c',
        'src/warpmv.h',
        'src/wedge.c',
        'src/wedge.h',
      ],
      'dependencies': [
        'libdav1d_bitdepth16',
        'libdav1d_bitdepth8',
        'libdav1d_entrypoint',
        'libdav1d_no_asm',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/libdav1d/',
          '<(DEPTH)/third_party/libdav1d/include',
          '<(DEPTH)/third_party/libdav1d/include/dav1d',
        ],
      },
    },
  ]
}
