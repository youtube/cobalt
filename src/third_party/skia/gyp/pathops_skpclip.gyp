# GYP file to build pathops skp clip test.
{
  'includes': [
    'apptype_console.gypi',
  ],
  'targets': [
    {
      'target_name': 'pathops_skpclip',
      'type': 'executable',
      'include_dirs': [
        '../src/core',
        '../src/effects',
        '../src/lazy',
        '../src/pathops',
        '../src/pipe/utils',
        '../src/utils',
      ],
      'dependencies': [
        'flags.gyp:flags',
        'skia_lib.gyp:skia_lib',
        'tools.gyp:crash_handler',
        'tools.gyp:resources',
      ],
      'sources': [
		'../tests/PathOpsDebug.cpp',
        '../tests/PathOpsSkpClipTest.cpp',
        '../src/utils/SkTaskGroup.cpp',
      ],
      'conditions': [
        [ 'skia_android_framework == 1', {
          'libraries': [
            '-lskia',
          ],
          'libraries!': [
            '-lz',
            '-llog',
          ],
        }],
        [ 'skia_gpu == 1', {
          'include_dirs': [
            '../src/gpu',
          ],
        }],
      ],
    },
  ],
}
