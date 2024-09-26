(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style type='text/css'>
    #node.anim {
        animation: anim 300ms ease-in-out paused;
    }

    @keyframes anim {
        from {
            width: 100px;
        }
        to {
            width: 200px;
        }
    }
    </style>
    <div id='node' style='background-color: red; width: 100px'></div>
  `, 'Test that css animation can be paused over protocol.');


  dp.Animation.enable();
  session.evaluate('node.classList.add("anim");');
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationCanceled();
  testRunner.log('Animation canceled');
  testRunner.completeTest();
})
