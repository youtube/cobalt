# Copyright 2019 Google Inc. All Rights Reserved.
{
  'variables': {
    'boringssl_root%': '<(DEPTH)/third_party/boringssl/src',
  },
  'targets': [
    {
      'target_name': 'crypto_tool',
      'type': '<(final_executable_type)',
      'include_dirs': [
        '<(boringssl_root)/include',
      ],
      'defines': [
        'OPENSSL_NO_SOCK'
      ],
      'sources' : [
        '<(boringssl_root)/tool/args.cc',
        '<(boringssl_root)/tool/ciphers.cc',
        '<(boringssl_root)/tool/const.cc',
        '<(boringssl_root)/tool/digest.cc',
        '<(boringssl_root)/tool/file.cc',
        '<(boringssl_root)/tool/generate_ed25519.cc',
        '<(boringssl_root)/tool/genrsa.cc',
        '<(boringssl_root)/tool/pkcs12.cc',
        '<(boringssl_root)/tool/rand.cc',
        '<(boringssl_root)/tool/sign.cc',
        '<(boringssl_root)/tool/speed.cc',
        '<(boringssl_root)/tool/tool.cc',
      ],
      'dependencies' : [
          '<(DEPTH)/third_party/boringssl/boringssl.gyp:crypto',
          '<(DEPTH)/starboard/starboard.gyp:starboard'
      ]
    },
    {
      'target_name': 'crypto_tool_deploy',
      'type': 'none',
      'dependencies': [
        'crypto_tool',
      ],
      'variables': {
        'executable_name': 'crypto_tool',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}
