module.exports = {
<<<<<<< HEAD
  env: {
    browser: true,
    es2021: true,
    node: true,
  },
  extends: ['google'],
  parser: '@typescript-eslint/parser',
  parserOptions: {
    ecmaVersion: 'latest',
    sourceType: 'module',
    project: './tsconfig.json',
  },
  plugins: ['@typescript-eslint'],
  rules: {
    // We don't want to enforce jsdoc everywhere:
    'require-jsdoc': 'off',

    // Relax jsdoc requirements
    'valid-jsdoc': [
      'error',
      {
        requireParamType: false,
        requireReturnType: false,
        requireReturn: false,
      },
    ],

    // Formatting handled by prettier
    'indent': 'off',
    'max-len': 'off',
    'operator-linebreak': 'off',
    'quotes': 'off',
    'brace-style': 'off',
    'space-before-function-paren': 'off',
    'generator-star-spacing': 'off',
    'semi-spacing': 'off',

    // clang-format --js used to format EOL comments after (e.g.) an if like:
=======
  'env': {
    'browser': true,
    'es2021': true,
    'node': true,
  },
  'extends': [
    'google',
  ],
  'parser': '@typescript-eslint/parser',
  'parserOptions': {
    'ecmaVersion': 'latest',
    'sourceType': 'module',
  },
  'plugins': [
    '@typescript-eslint',
  ],
  'rules': {
    // We don't want to enforce jsdoc everywhere:
    'require-jsdoc': 'off',

    // Max line length is 80 with 2 space tabs. This must match the
    // ui/.clang-format definition:
    'max-len': [
      'error',
      {
        'code': 80,
        'tabWidth': 2,
        'ignoreUrls': true,
        'ignoreTemplateLiterals': true,
        'ignoreStrings': true,
      },
    ],

    // Indentation handled by clang-format --js:
    'indent': 'off',

    // clang-format --js formats EOL comments after (e.g.) an if like:
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    // if (foo) {  // insightful comment
    // with two spaces between the slash and the brace. Turn
    // ignoreEOLComments on to allow that. We still want
    // no-multi-spaces turned on in general as it fixes issues like:
    // if (a ===   b)
    'no-multi-spaces': ['error', {ignoreEOLComments: true}],

    // Default no-unused-vars doesn't understand TypeScript enums. See:
    // https://github.com/typescript-eslint/typescript-eslint/issues/2621
    'no-unused-vars': 'off',
<<<<<<< HEAD
    '@typescript-eslint/no-unused-vars': [
      'error',
      {argsIgnorePattern: '^_.*', varsIgnorePattern: '^_.*'},
    ],
=======
    '@typescript-eslint/no-unused-vars':
        ['error', {'argsIgnorePattern': '^_.*', 'varsIgnorePattern': '^_.*'}],
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    // new Array() is banned (use [] instead) but new Array<Foo>() is
    // allowed since it can be clearer to put the type by the
    // construtor.
    'no-array-constructor': 'off',
    '@typescript-eslint/no-array-constructor': ['error'],

    // Rest parameters are not equivalent to 'arguments'.
    // Rest parameters are arrays: https://developer.mozilla.org/en-US/docs/Web/
    // JavaScript/Reference/Functions/rest_parameters
    // 'arguments' are objects: https://developer.mozilla.org/en-US/docs/Web/
    // JavaScript/Reference/Functions/arguments
    'prefer-rest-params': 'off',

    // We have a lot normal functions which are capitalised.
    // TODO(hjd): Switch these to be lowercase and remove capIsNew.
    // There are also some properties like: foo.factory these should
    // stay.
<<<<<<< HEAD
    'new-cap': ['error', {capIsNew: false, properties: false}],

    // Don't allow new introduction of any it is most always a mistake.
    '@typescript-eslint/no-explicit-any': 'error',

    // Prohibit numbers and strings from being used in boolean expressions.
    '@typescript-eslint/strict-boolean-expressions': [
      'error',
      {
        // Eventually we probably want to enable all of these, for now this
        // tackles numbers and keeps the error count manageable.
        allowAny: true,
        allowNullableBoolean: true,
        allowNullableString: true,
        allowNumber: true,
        allowString: true,
      },
    ],
=======
    'new-cap': ['error', {'capIsNew': false, 'properties': false}],
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  },
};
