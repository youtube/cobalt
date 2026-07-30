// Runs the test case where a hidden iframe tries play media while it is
// disallowed by the media-playback-while-not-visible permission policy.
function runCantPlayIfHiddenCrossOriginTest(frameType, visibilityType) {
  const label = frameType === 'nested' ? 'nested ' : '';
  promise_test(
      async t => {
        const iframe = await setUpCrossOriginIframe(t, frameType);

        hideFrame(iframe, visibilityType);

        // Attempting to play while hidden should fail with NotAllowedError.
        // Visibility propagation to a cross-origin frame may take some time,
        // so retry until the play call is correctly rejected.
        await t.step_wait(
            async () => await playMediaInIframe(iframe) === 'NotAllowedError',
            'waiting for play to be rejected');
        assert_equals(await queryPlayerStatus(iframe), 'Paused');
      },
      'Cross-origin media elements in non-rendered ' + label +
          'iframes shouldn\'t be allowed to play (type = ' + visibilityType +
          ')');
}

// Runs the test case where an iframe that is disallowed by the
// media-playback-while-not-visible permission policy becomes hidden while
// playing media, and then becomes visible again.
function runPauseWhenHiddenCrossOriginTest(frameType, visibilityType) {
  const label = frameType === 'nested' ? 'nested ' : '';
  promise_test(
      async t => {
        const iframe = await setUpCrossOriginIframeAndPause(t, frameType);

        // Play the media while visible.
        assert_equals(await playMediaInIframe(iframe), 'Success');

        // Hiding the iframe should pause the embedded video element playback.
        let stateChangePromise = expectMediaPlayerStateChangeInIframe();
        hideFrame(iframe, visibilityType);
        assert_equals(await stateChangePromise, 'Pause');

        // Attempting to play while hidden should fail.
        assert_equals(await playMediaInIframe(iframe), 'NotAllowedError');
        assert_equals(await queryPlayerStatus(iframe), 'Paused');

        // When the iframe is shown again, playing should be allowed.
        // Visibility propagation to a cross-origin frame may take some time,
        // so retry the play call until it succeeds.
        stateChangePromise = expectMediaPlayerStateChangeInIframe();
        showFrame(iframe, visibilityType);
        await t.step_wait(
            async () => await playMediaInIframe(iframe) === 'Success',
            'waiting for play to succeed');
        assert_equals(await stateChangePromise, 'Play');
      },
      'Cross-origin media elements in non-rendered ' + label +
          'iframes should pause (type = ' + visibilityType + ')');
}
