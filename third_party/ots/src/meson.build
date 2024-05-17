project('ots', 'c', 'cpp',
  version: '8.2.1',
  default_options : ['cpp_std=c++11', 'default_library=static', 'warning_level=2'],
  meson_version : '>= 0.55.2',
)

subdir('tests')

cxx = meson.get_compiler('cpp')

conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())

if get_option('debug')
  conf.set('OTS_DEBUG', 1)
endif

if get_option('graphite')
  conf.set('OTS_GRAPHITE', 1)
endif

freetype = dependency('freetype2', required: false)
if freetype.found()
  conf.set('HAVE_FREETYPE', 1)
endif

coretext = dependency('appleframeworks', modules: 'applicationservices',
                      required: false)
if coretext.found()
  conf.set('HAVE_CORETEXT', 1)
endif

gdi32 = cxx.find_library('gdi32', required: false)
if gdi32.found()
  conf.set('HAVE_WIN32', 1)
endif


configure_file(output: 'config.h',
               configuration: conf)


ots_sources = [
  'src/avar.cc',
  'src/avar.h',
  'src/cff.cc',
  'src/cff.h',
  'src/cff_charstring.cc',
  'src/cff_charstring.h',
  'src/cmap.cc',
  'src/cmap.h',
  'src/cvar.cc',
  'src/cvar.h',
  'src/cvt.cc',
  'src/cvt.h',
  'src/fpgm.cc',
  'src/fpgm.h',
  'src/fvar.cc',
  'src/fvar.h',
  'src/gasp.cc',
  'src/gasp.h',
  'src/gdef.cc',
  'src/gdef.h',
  'src/glyf.cc',
  'src/glyf.h',
  'src/gpos.cc',
  'src/gpos.h',
  'src/gsub.cc',
  'src/gsub.h',
  'src/gvar.cc',
  'src/gvar.h',
  'src/hdmx.cc',
  'src/hdmx.h',
  'src/head.cc',
  'src/head.h',
  'src/hhea.cc',
  'src/hhea.h',
  'src/hmtx.h',
  'src/hvar.cc',
  'src/hvar.h',
  'src/kern.cc',
  'src/kern.h',
  'src/layout.cc',
  'src/layout.h',
  'src/loca.cc',
  'src/loca.h',
  'src/ltsh.cc',
  'src/ltsh.h',
  'src/math.cc',
  'src/math_.h',
  'src/maxp.cc',
  'src/maxp.h',
  'src/metrics.cc',
  'src/metrics.h',
  'src/mvar.cc',
  'src/mvar.h',
  'src/name.cc',
  'src/name.h',
  'src/os2.cc',
  'src/os2.h',
  'src/ots.cc',
  'src/ots.h',
  'src/post.cc',
  'src/post.h',
  'src/prep.cc',
  'src/prep.h',
  'src/stat.cc',
  'src/stat.h',
  'src/variations.cc',
  'src/variations.h',
  'src/vdmx.cc',
  'src/vdmx.h',
  'src/vhea.cc',
  'src/vhea.h',
  'src/vmtx.h',
  'src/vorg.cc',
  'src/vorg.h',
  'src/vvar.cc',
  'src/vvar.h',
]

zlib = dependency('zlib', fallback : ['zlib', 'zlib_dep'])

libwoff2dec = dependency('libwoff2dec',
                         fallback: ['google-woff2', 'woff2_decoder_dep'])

ots_deps = [zlib, libwoff2dec]

if get_option('graphite')
  ots_sources += [
    'src/feat.cc',
    'src/feat.h',
    'src/glat.cc',
    'src/glat.h',
    'src/gloc.cc',
    'src/gloc.h',
    'src/graphite.h',
    'src/sile.h',
    'src/sile.cc',
    'src/silf.h',
    'src/silf.cc',
    'src/sill.h',
    'src/sill.cc',
  ]
  liblz4 = dependency('liblz4', fallback: ['lz4', 'liblz4_dep'])
  ots_deps += [liblz4]
endif

libots = library('ots',
  ots_sources,
  include_directories: include_directories('include'),
  cpp_args : '-DHAVE_CONFIG_H',
  dependencies: ots_deps,
)


ots_sanitize = executable('ots-sanitize',
  'util/ots-sanitize.cc',
  'util/test-context.h',
  include_directories: include_directories('include'),
  link_with: libots,
  install: true,
)
install_man('docs/ots-sanitize.1')


fuzzer_ldflags = []
fuzzer_defines = []
if get_option('fuzzer_ldflags') != ''
  fuzzer_ldflags += get_option('fuzzer_ldflags').split()
  fuzzer_defines += ['-DOTS_FUZZER_NO_MAIN']
endif

ots_fuzzer = executable('ots-fuzzer',
  'util/ots-fuzzer.cc',
  include_directories: include_directories(['include', 'src']),
  cpp_args: fuzzer_defines,
  link_with: libots,
  link_args: fuzzer_ldflags,
)


if freetype.found() or coretext.found() or gdi32.found()
  deps = [freetype, gdi32]
  if coretext.found()
    deps += [coretext]
  endif
  ots_idempotent = executable('ots-idempotent',
    'util/ots-idempotent.cc',
    'util/test-context.h',
    include_directories: include_directories('include'),
    link_with: libots,
    dependencies: deps,
    install: true,
  )
  install_man('docs/ots-idempotent.1')
  foreach file_name : good_fonts
    test(file_name, ots_idempotent,
      args: meson.current_source_dir() / 'tests' / file_name,
      suite: 'good',
    )
  endforeach
endif


if freetype.found() or coretext.found()
  deps = [freetype]
  if coretext.found()
    deps += [coretext]
  endif
  executable('ots-validator-checker',
    'util/ots-validator-checker.cc',
    include_directories: include_directories('include'),
    link_with: libots,
    dependencies: deps,
    install: true,
  )
  install_man('docs/ots-validator-checker.1')
endif


if freetype.found()
  executable('ots-side-by-side',
    'util/ots-side-by-side.cc',
    include_directories: include_directories('include'),
    link_with: libots,
    dependencies: freetype,
    install: true,
  )
  install_man('docs/ots-side-by-side.1')
endif


if cxx.has_header('sys/time.h')
  executable('ots-perf',
    'util/ots-perf.cc',
    include_directories: include_directories('include'),
    link_with: libots,
    install: true,
  )
  install_man('docs/ots-perf.1')
endif


# https://mesonbuild.com/Dependencies.html#gtest-and-gmock
gtest = dependency('gtest', main : true, fallback : ['gtest', 'gtest_main_dep'])

cff_charstring = executable('cff_charstring',
  'tests/cff_charstring_test.cc',
  include_directories: include_directories(['include', 'src']),
  link_with: libots,
  dependencies: gtest,
)

test('cff_charstring', cff_charstring)


layout_common_table = executable('layout_common_table',
  'tests/layout_common_table_test.cc',
  include_directories: include_directories(['include', 'src']),
  link_with: libots,
  dependencies: gtest,
)

test('layout_common_table', layout_common_table,
  protocol: 'gtest',
)


foreach file_name : bad_fonts
  test(file_name, ots_sanitize,
    args: meson.current_source_dir() / 'tests' / file_name,
    suite: 'bad',
    should_fail: true,
  )
endforeach


foreach file_name : fuzzing_fonts
  test(file_name, ots_fuzzer,
    args: meson.current_source_dir() / 'tests' / file_name,
    suite: 'fuzzing',
  )
endforeach
