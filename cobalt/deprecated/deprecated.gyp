{
  'targets': [
    {
      'target_name': 'platform_delegate',
      'type': '<(component)',
      'toolsets': ['target'],

      'include_dirs': [
        '<(DEPTH)',
      ],

      'sources': [
        'platform_delegate.cc',
        'platform_delegate.h',
        'platform_delegate_<(target_arch).cc',
      ],
    },
  ],
}
