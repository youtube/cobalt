{
  'includes': ['release_defaults.gypi'],
  'defines': ['OFFICIAL_BUILD'],
  'msvs_settings': {
    'VCCLCompilerTool': {
      'Optimization': '3',
      'InlineFunctionExpansion': '2',
      'EnableIntrinsicFunctions': 'true',
      'FavorSizeOrSpeed': '2',
      'OmitFramePointers': 'true',
      'EnableFiberSafeOptimizations': 'true',
      'WholeProgramOptimization': 'true',
    },
    'VCLibrarianTool': {
      'AdditionalOptions': ['/ltcg', '/expectedoutputsize:120000000'],
    },
    'VCLinkerTool': {
      # Get more debug spew from the linker while we're sorting out
      # build problems and performance.
      # TODO(siggi): Remove these flags after we're out of the woods.
      'AdditionalOptions': ['/verbose', '/time'],
      'LinkTimeCodeGeneration': '1',
      # The /PROFILE flag causes the linker to add a "FIXUP" debug stream to
      # the generated PDB. According to MSDN documentation, this flag is only
      # available (or perhaps supported) in the Enterprise (team development)
      # version of Visual Studio. If this blocks your official build, simply
      # comment out this line, then  re-run "gclient runhooks".
      'Profile': 'true',
    },
  },
}
