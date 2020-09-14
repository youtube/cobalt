{
  'targets': [
    {
      'target_name': 'cobalt_build_id',
      'type': 'none',
      # Because we generate a header, we must set the hard_dependency flag.
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'cobalt_build_id',
          'variables': {
            'build_id_py_path': 'build_id.py',
            'output_path': '<(SHARED_INTERMEDIATE_DIR)/cobalt_build_id.h',
          },
          'inputs': [
            '<(build_id_py_path)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': [
            'python2',
            '<(build_id_py_path)',
            '<(output_path)',
            '<(cobalt_version)',
          ],
          'message': 'Generating build information',
        },
      ],
    }
  ] # end of targets
}
