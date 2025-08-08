// META: script=resources/workaround-for-362676838.js
// META: script=resources/utils.js
// META: timeout=long

promise_test(async () => {
  const result = await testPromptAPI();
  assert_true(result.success, result.error);
});

// Test the creation options.
promise_test(async t => {
  // Make sure the prompt api is enabled.
  assert_true(!!ai);
  // Make sure the session could be created.
  const capabilities = await ai.languageModel.capabilities();
  const status = capabilities.available;
  assert_true(status !== 'no');

  let session;
  let result;

  // Create a new session with no option.
  session = await ai.languageModel.create();
  assert_true(!!session);

  // Create a new session with topK and temperature.
  session = await ai.languageModel.create({ topK: 3, temperature: 0.6 });
  assert_true(!!session);

  // Create a new session with only topK or temperature, it should fail.
  result = ai.languageModel.create({ topK: 3 });
  await promise_rejects_dom(
      t, 'NotSupportedError', result,
      'Initializing a new session must either specify both topK and temperature, or neither of them.');

  result = ai.languageModel.create({ temperature: 0.5 });
  await promise_rejects_dom(
      t, 'NotSupportedError', result,
      'Initializing a new session must either specify both topK and temperature, or neither of them.');

  // Create a new session with system prompt.
  session = await ai.languageModel.create({ systemPrompt: 'you are a robot' });
  assert_true(!!session);

  // Create a new session with initial prompts.
  session = await ai.languageModel.create({
    initialPrompts: [
      {role: 'system', content: 'you are a robot'},
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'}
    ]
  });
  assert_true(!!session);

  // Create a new session with initial prompts without system role.
  session = await ai.languageModel.create({
    initialPrompts: [
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'}
    ]
  });
  assert_true(!!session);

  // Create a new session with initial prompts with system role not placing as
  // the first element.
  result = ai.languageModel.create({
    initialPrompts: [
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'},
      {role: 'system', content: 'you are a robot'}
    ]
  });
  await promise_rejects_js(t, TypeError, result);

  // Create a new session with both system prompt and initial prompts, but the
  // initial prompts do not contain a system role, it should pass.
  result = ai.languageModel.create({
    systemPrompt: 'you are a robot',
    initialPrompts: [
      { role: 'user', content: 'hello' }, { role: 'assistant', content: 'hello' }
    ]
  });
  assert_true(!!session);

  // Create a new session with both system prompt and initial prompts, and the
  // initial prompts contain a system role, it should fail.
  result = ai.languageModel.create({
    systemPrompt: 'you are a robot',
    initialPrompts: [
      {role: 'system', content: 'you are a robot'},
      {role: 'user', content: 'hello'}, {role: 'assistant', content: 'hello'}
    ]
  });
  await promise_rejects_js(t, TypeError, result);
});
