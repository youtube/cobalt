{
  'targets': [
    {
      'target_name': 'webdriver_test',
      'type': '<(gtest_target_type)',
      'conditions': [
        ['enable_webdriver==1', {
          'sources': [
            'get_element_text_test.cc',
            'is_displayed_test.cc',
            'keyboard_test.cc',
            'execute_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'actions': [
        {
          'action_name': 'webdriver_test_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/webdriver/testdata/',
            ],
            'output_dir': 'cobalt/webdriver_test',
          },
          'includes': ['../build/copy_test_data.gypi'],
        }
      ],
    },

    {
      'target_name': 'webdriver_test_deploy',
      'type': 'none',
      'dependencies': [
        'webdriver_test',
      ],
      'variables': {
        'executable_name': 'webdriver_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ]
}
