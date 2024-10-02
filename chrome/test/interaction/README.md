# Interactive Testing API: "Kombucha"

**[go/kombucha-api](goto.google.com/kombucha-api)**

**Kombucha** is a group of powerful test mix-ins that let you easily and
concisely write interactive tests.

The current API version is 2.0. All future 2.x versions are guaranteed to
either be backwards-compatible with existing tests, or the authors will update
the API calls for you.

This page provides technical documentation. For a cookbook/FAQ/troubleshooting
guide, see our [Kombucha Playbook](goto.google.com/kombucha-playbook).

 - [Changelog](#changelog)
 - [Known Issues](#known-issues-and-incompatibilities)

[TOC]

## Getting Started

There are two ways to write a Kombucha-based interaction test:
1. Alias or inherit from one of our pre-configured test fixtures (preferred):
    - [InteractiveTest](/ui/base/interaction/interactive_test.h)
    - [InteractiveViewsTest](/ui/views/interaction/interactive_views_test.h)
    - [InteractiveBrowserTest](/chrome/test/interaction/interactive_browser_test.h)
2. Have your test fixture inherit the appropriate Kombucha API class:
    - [InteractiveTestApi](/ui/base/interaction/interactive_test.h)
    - [InteractiveViewsTestApi](/ui/views/interaction/interactive_views_test.h)
    - [InteractiveBrowserTestApi](/chrome/test/interaction/interactive_browser_test.h)

If you go the latter route, please see
[Custom Test Fixtures](#custom-test-fixtures) below.

## Using the Kombucha API

***Note:** Throughout this section, unless otherwise specified, all methods are
present in `InteractiveTestApi`. If a method is introduced in
`InteractiveViewsTestApi`, it will have **[Views]** next to it; if it's
introduced in `InteractiveBrowserTestApi`, it will have **[Browser]** next to it
instead.*

### Test Sequences

The primary entry point for any test is `RunTestSequence()` [Views] or
`RunTestSequenceInContext()`. (For more information on `ElementContext`, see the
[Interaction Library Documentation](/ui/base/interaction/README.md).)

`RunTestSequence()` is designed to accept any number of steps. You will use the
provided palette of test verbs and checks that the API provides, or [create your
own verbs](#custom-verbs) through generator methods. The steps are executed in
order until either the final step completes, which is considered success, or a
step fails (or the test times out), which are considered failures.

Example:

```cpp
class MyDialogTest : public InteractiveBrowserTest { ... };

IN_PROC_BROWSER_TEST_F(MyDialogTest, Apply) {
  RunTestSequence(
      // Open my dialog.
      PressButton(kShowMyDialogButtonId),
      WaitForShow(kMyDialogId),
      // Validate the caption of the Apply button.
      CheckViewProperty(kApplyButtonId, &LabelButton::GetText, u"Apply"),
      // Press the button and verify that the state is applied.
      PressButton(kApplyButtonId),
      // This is a custom verb created for this test suite.
      VerifyChangesApplied());
}
```

### Test Verbs

Kombucha *test verbs* are methods that you can call in your test which generate
test steps. Most verbs take an `ElementSpecifier` - either an ID or a name -
describing which UI element the verb should be applied to, but not all do. Some
verbs, like `Check()` and `Do()` don't care about specific elements.

Verbs fall into a number of different categories:
- **Do** performs an action you specify.
- **Log** prints its arguments to the output at log level `INFO`.
  See [Logging](#logging) below.
- **Check** verbs ensure that some condition is true; if it is not, the test
  fails. Some *Check* verbs use `Matcher`s, some use callbacks, etc. Examples
  include:
    - `Check()`
    - `CheckResult()`
    - `CheckElement()`
    - `CheckView()` [Views]
    - `CheckViewProperty()` [Views]
    - `Screenshot` [Browser] - compares the target against Skia Gold in pixel
      tests. See [Handling Incompatibilities](#handling-incompatibilities) for
      how to handle this in non-pixel tests.
- **WaitFor** verbs ensure that the given UI event happens or condition becomes
  true before proceeding. Examples:
    - `WaitForShow()`
    - `WaitForHide()`
    - `WaitForActivated()`
    - `WaitForEvent()`
- **After** verbs allow you to take some action when a given event takes place
  or condition becomes true. The action can be a full
  `InteractionSequence::StepStartCallback` or it can omit any number of leading
  arguments; try to be as concise as possible. Examples:
    - `AfterShow()`
    - `AfterHide()`
    - `AfterActivated()`
    - `AfterEvent()`
- **With** verbs get the specified element and perform the specified action.
  Unlike the above verbs, they will not wait; the element must exist when the step
  triggers or the test will fail.
    - `WithElement()`
    - `WithView()` [Views]
- **Ensure** verbs check the presence or absence of an element after allowing
  all pending events to settle. They are not compatible with `InAnyContext()`
  for technical reasons, and therefore, take an `in_any_context` parameter.
  There are also versions that look for a DOM element in an
  [instrumented WebContents](#webcontents-instrumentation) [Browser].
    - `EnsurePresent()`
    - `EnsureNotPresent()`
- **Action** verbs simulate input to specific UI elements. You can often specify
  the type of input you want to simulate (keyboard, mouse, etc.) but you don't
  have to. Some of these (`ActivateSurface()`, `SendAccelerator()`) may flake in
  environments where the test fixture is not running as the only process, so
  prefer to use those in interactive_ui_tests. Examples:
    - `PressButton()`
    - `SelectMenuItem()`
    - `SelectTab()`
    - `SelectDropdownItem()`
    - `EnterText()`
    - `SendAccelerator()`
    - `Confirm()`
    - `DoDefaultAction()`
    - `ActivateSurface()`
      - ActivateSurface is not always reliable on Linux with the Wayland window
        manager; see [Handling Incompatibilities](#handling-incompatibilities)
        for how to correctly deal with this.
    - `ScrollToVisible()` [Views, Browser]
      - Recommended before doing anything that needs the screen coordinates of
        a UI or DOM element that is in a scrollable container.
- **Mouse** verbs simulate mouse input to the entire application, and are
  therefore only reliable in test fixtures that run as exclusive processes (e.g.
  interactive_browser_tests). Examples include:
    - `MoveMouseTo()` [Views]
    - `DragMouseTo()` [Views]
    - `ClickMouse()` [Views]
    - `ReleaseMouseButton()` [Views]
- **Name** verbs assign a string name to some UI element which may not be known
  ahead of time, so that it can be referenced later in the test. Examples
  include:
    - `NameView()` [Views]
    - `NameChildView()` [Views]
    - `NameChildViewByType()` [Views]
    - `NameDescendantView()` [Views]
    - `NameDescendantViewByType()` [Views]
    - `NameViewRelative()` [Views]
- **WebContents** verbs either dynamically
  [instrument WebContents](#webcontents-instrumentation), navigate them, or wait
  for them to navigate or change state.
    - `InstrumentTab()` [Browser]
    - `InstrumentNextTab()` [Browser]
    - `AddInstrumentedTab()` [Browser]
    - `InstrumentNonTabWebView()` [Browser]
    - `NavigateWebContents()` [Browser]
    - `WaitForWebContentsReady()` [Browser]
    - `WaitForWebContentsNavigation()` [Browser]
    - `WaitForStateChange()` [Browser]
- **Javascript** verbs execute javascript in an
  [instrumented WebContents](#webcontents-instrumentation), or verify a result
  from calling a javascript function. The `*At()` methods take a
  [DeepQuery](#specifying-dom-elements) and operate on a specific DOM element
  (possibly in a Shadow DOM), while the non-at methods operate at global scope.
  If you are not sure if the target element exists or the condition is true yet,
  use `WaitForStateChange()` instead. Examples:
   - `ExecuteJs()` [Browser]
   - `ExecuteJsAt()` [Browser]
   - `CheckJsResult()` [Browser]
   - `CheckJsResultAt()` [Browser]
- **Utility** verbs modify how the test sequence is executed.
   - `FlushEvents()` - ensures that the next step happens on a fresh
     message loop rather than being able to chain successive steps.
   - `SetOnIncompatibleAction()` changes what the sequence will do when faced
     with an action that cannot be executed on the current
     build, environment, or platform. See
     [Handling Incompatibilities](#handling-incompatibilities) for more
     information and best practices.

Example with mouse input:
```cpp
// Navigate a page, click Back, and verify that the page navigates back
// correctly.
RunTestSequence(
    NavigateWebContents(kActiveTabId, kTargetUrl),
    MoveMouseTo(kBackButtonId),
    ClickMouse(),
    WaitForWebContentsNavigation(kPreviousUrl));
```

Example with a named element:
```cpp
RunTestSequence(
    // Identify the first child view of the button container.
    NameChildView(kDialogButtons, kFirstButton, 0),
    // Verify that this is the OK button.
    CheckViewProperty(kFirstButton,
                      &LabelButton::GetText,
                      l10n_util::GetStringUTF16(IDS_OK)),
    // Press the button.
    PressButton(kFirstButton));
```

### Test Functions and Callbacks

Many verbs and modifiers, such as `Do`, `After...`, `With...`, `Check...`, and
`If...` take a test function or callback as an argument.

Kombucha allows you to specify these functions in whatever way is clearest and
most concise. You may use any of the following:
 - A callback (resulting from `base::Bind...()`)
 - A function pointer
 - A bare lambda or reference to a lambda, with or without bound arguments

The following are, therefore, all valid:
```cpp
void Func() {
  // ...
}

IN_PROC_BROWSER_TEST_F(MyTest, TestDo) {
  auto lambda = [](){ Func(); };
  auto once_callback = base::BindOnce(&Func);
  auto repeating_callback = base::BindRepeating(&Func);
  int x = 1;
  int y = 2;
  RunTestSequence(
      Do(&Func),
      Do(lambda),
      Do(std::move(once_callback)),
      Do(repeating_callback),
      Do([x, &y](){ Func(), LOG(INFO) << "Bound args " << x << ", " << y; }));
}
```

Note that a few cases do still require you to use `base::Bind...`; specifically,
the arguments to actions like `NameChildView` and `NameDescendantView`. When a
verb does require an explicit argument it will be provided in the verb's method
signature.

### Logging

Using the `Log` verb allows for printing of any number of arguments. They are
sent to log level `INFO` when the `Log` step is executed. `Log` "knows" how to
print anything that our logging macros can. So if you can do
`LOG(INFO) << value` you can print it with the `Log` verb.

There are a few different ways to pass values to `Log`:
 - If you just pass a variable or literal, the value that is printed is the
   value _at the time the sequence is created_.
 - If you wrap a variable with `std::ref`, the value that is printed is the
   value of the variable _at the time the `Log` step is executed_.
 - You can also pass any callable object (callback, lambda, or function
   pointer) that returns a loggable value. The callable object is executed when
   the `Log` step runs and the result is printed.

Example:
```cpp
int x = 1;
RunTestSequence(
  // Change the value of x.
  Do([&](){ ++x; }),
  // Print out old, current, and computed values.
  Log("Original value: ", x,
      " current value: ", std::ref(x),
      " square of current value: ", [&x](){ return x*x; }));
```

### Modifiers

A modifier wraps around a step or steps and change their behavior.

- **InAnyContext** allows the modified verb to find an element outside the test's default
  `ElementContext`. Unlike the other modifiers, there are a number of limitations on its use:
  - It should not be used with `FlushEvents`, most `Ensure`, or any `Activate`,
    `Event`, or `Mouse` verbs.
    - This is a shortcoming in the underlying framework that will be fixed in the future.
  - It should not be used with named elements, which can already be found in any context.
  - For unsupported verbs, it is best to either use `InSameContext()` or `InContext()` instead.
  - Example:

```cpp
RunTestSequence(
    // This button might be in a different window!
    InAnyContext(PressButton(kMyButton)),
    InAnyContext(CheckView(kMyButton, ensure_pressed)));
```

- **InSameContext** allows the modified verb (or verbs) to find an element in the same context
  as the previous step.
  - Has no effect on `EnsurePresent()` or `EnsureNotPresent()` when the `in_any_context`
    parameter is set to true.
  - Example:
```cpp
RunTestSequence(
    InAnyContext(WaitForShow(kMyButton)),
    InSameContext(PressButton(kMyButton)));
```

- **InContext** allows the modified verb (or verbs) to execute in the specified context instead of
  the default context for the sequence.
  - Has no effect on `EnsurePresent()` or `EnsureNotPresent()` when the `in_any_context` parameter
    is set to true.
  - Example:

```cpp
Browser* const incognito = CreateIncognitoBrowser();
RunTestSequence(
  /* Do stuff in primary browser context here */
  /* ... */
  InContext(incognito->window()->GetElementContext(), Steps(
    PressButton(kAppMenuButton),
    WaitForShow(kDownloadsMenuItemElementId))));
```

### Control Flow

Kombucha now provides two options for control flow:
 - Conditionals
 - Parallel execution

#### Conditionals

In some cases, you may want to execute part of a test only if, for example, a
particular flag is set. In order to do this, we provide the various `If()`
control-flow statements:
 - `If(condition, then_steps[, else_steps])` - executes `then_steps`, which can
   be a single step or a `MultiStep`, if `condition` returns true. If
   `else_steps` is present, it will be executed if `condition` returns false.
 - `IfMatches(function, matcher, then_steps[, else_steps])` - same as above
   but `then_steps` executes if the result of `function` matches `matcher`.
 - `IfElement()`, `IfElementMatches()` - same as above, but the `condition` or
   `function` receives a const pointer to the specified element as an argument.
   If the element is not visible, the condition receives `nullptr` (it does not
   fail).
 - `IfView()`, `IfViewMatches()` - same as above, except that the condition
   takes a const pointer to a `View` or `View` subclass; if the element is not
   present, null is passed, but if it is the wrong type, the test fails.
 - `IfViewPropertyMatches()` - same as above, but you specify a readonly method
   on the View rather than an arbitrary function. Syntax is similar to
   `CheckViewProperty()`.

Example:
```cpp
RunTestSequence(
  /* ... */
  // If MyFeature is enabled, it may interfere with the rest of this test, so
  // toggle its UI off:
  If(base::Bind(&base::FeatureList::IsEnabled, kMyFeature)),
     Steps(PressButton(kFeatureToggleButtonElementId),
           WaitForHide(kMyFeatureUiElementId)),
  /* Proceed with test... */
)
```

Note that in the case of elements, if the element isn't present/visible, the
step does not fail; `condition` will simply receive a null value.
```cpp
RunTestSequence(
  /* ... */
  // If the side panel is still visible, close it.
  IfView(kSidePanelElementId,
         // If the side panel is visible...
         [](const SidePanel* side_panel) { return side_panel != nullptr; },
         // Then press the side panel button to close the side panel.
         Steps(PressButton(kSidePanelButtonElementId),
               WaitForHide(kSidePanelElementId)),
         // Else note that it was not open.
         Log("Side panel was already closed.")),
  /* ... */
)
```

Matchers are straightforward; consider the following case where we want to open
a new tab, but only if there are fewer than two tabs open:
```cpp
RunTestSequence(
  /* ... */
  IfMatches(
      // If there are fewer than two tabs...
      [this]() { return browser()->tab_strip_model()->count(); },
      testing::Lt(2),
      // Then open a new tab:
      PressButton(kNewTabButtonElementId)),
  /* ... */
)
```

#### Parallel Execution

Another common case you might want to handle in a test is when multiple events
are going to happen, but you can't guarantee the exact order. Because Kombucha
tests are sequential, if a test needs to respond to two discrete events with
non-deterministic timing, you need to be able to execute multiple steps in
parallel.

For this, we provide `InParallel()` and `AnyOf()`:
 - `InParallel(step[s], step[s], ...)` - Executes each of `step[s]` in parallel
   with each other. All must complete before the main test sequence can proceed.
 - `AnyOf(step[s], step[s], ...)` - Executes each of `step[s]` in parallel with
   each other. Only one must complete, at which point the main test sequence
   proceeds and the other sequences are scuttled.

Example:
```cpp
RunTestSequence(
  /* ... */
  // This button press will cause two asynchronous processes to spawn.
  PressButton(kStartBackgroundProcessesButtonElementId),
  InParallel(
    WaitForEvent(kMyFeatureUiElementID, kUserDataUpdatedEvent),
    WaitForEvent(kMyFeatureUiElementId, kUiUpdated)),
  // It's now safe to proceed.
  /* ... */
)
```

#### Control Structure Usage Notes and Limitations

Avoid executing steps with side-effects during an `InParallel()` or `AnyOf()`,
especially if those steps could affect other subsequences running in parallel.

Avoid relying on any side-effects of a step in an `If()` or `AnyOf()` in the
remainder of the test, as there is no guarantee those steps will be executed
(or in the case of `AnyOf()`, they may be executed non-deterministically, which
is worse). For example:

```cpp
RunTestSequence(
  /* ... */
  AnyOf(
    // WARNING: One or both of these buttons will be pressed, but which is not
    // deterministic!
    Steps(WaitForShow(kMyElementId1), PressButton(kMyButtonId1)),
    Steps(WaitForShow(kMyElementId2), PressButton(kMyButtonId2)))
)
```

Triggering conditions for the first step of a conditional or parallel
subsequence can occur during the previous step. However, the triggering
condition for the first step of the main test sequence _following_ the control
structure cannot occur during subsequence execution (it will be lost).
For example:

```cpp
RunTestSequence(
  /* ... */
  PressButton(kButtonElementId),
  InParallel(
    // This is okay, since the first step of a subsequence can trigger during
    // the previous step.
    WaitForActivate(kButtonElementId),
    Steps(WaitForEvent(kButtonElementId, kBackgroundProcessEvent),
          PressButton(kOtherButtonElementId))),
  // WARNING: This is unsafe as the PressButton() above occurs in a subsequence,
  // but this action is in the main sequence.
  WaitForActivate(kOtherButtonElementId)
)
```

Named elements are inherited by conditional or parallel subsequences, but any
names that are assigned by the subsequence are not guaranteed to be brought back
to the top level test sequence. **We may change this behavior in the future.**

### Handling Incompatibilities

Sometimes a test won't run on a specific build bot or in a specific environment
due to a known incompatibility (as opposed to something legitimately failing).
See [Known Incompatibilities](#known-issues-and-incompatibilities) for more
info.

Normally, if you know that the test won't run on an entire platform (i.e. you
can use `BUILDFLAG()` to differentiate) you should disable or skip the tests in
the usual way. But if the distinction is finer-grained (as with the above verbs)
The `SetOnIncompatibleAction()` verb and `OnIncompatibleAction` enumeration are
provided.
 - `OnIncompatibleAction::kFailTest` is the default option; if a step fails
   because of a known incompatibility, the test will fail, and an error message
   will be printed.
 - `OnIncompatibleAction::kSkipTest` immediately skips the test as soon as an
   incompatibility is detected. Use this option when you know the rest of the
   test will fail and the test results are invalid. A warning will be printed.
 - `OnIncompatibleAction::kHaltTest` immediately halts the sequence but does not
   fail or skip the test. Use this option when all of the steps leading up to
   the incompatible one are valid and you want to preserve any non-fatal errors
   that may have occurred. A warning will still be printed.
 - `OnIncompatibleAction::kIgnoreAndContinue` skips the problematic step, prints
   a warning, and continues the test as if nothing happened. Use this option
   when the step is incidental to the test, such as taking a screenshot in the
   middle of a sequence.

***Do not use `SetOnIncompatibleAction()` unless:***
 1. You know the test will fail due to a known incompatibility.
 2. The test cannot be disabled or skipped using a simple `BUILDFLAG()` check.

Note that you *must* specify a non-empty `reason` when calling
`SetOnIncompatibleAction()` with any argument except `kFailTest`. This string
will be printed out as part of the warning that is produced if the step fails.

### WebContents Instrumentation

A feature of `InteractiveBrowserTestApi` that it borrows from
[WebContentsInteractoinTestUtil](/chrome/test/interaction/webcontents_interaction_test_util.h)
is the ability to *instrument* a `WebContents`. This does the following:
- Assigns the entire `WebContents` a unique `ElementIdentifier`.
- Enables a number of page navigation verbs, such as `NavigateWebContents()`
  and `WaitForWebContentsReady()`.
- Allows the execution of arbitrary JS in the WebContents.
- Allows waiting for a specific condition in the DOM of the `WebContents` via
  `WaitForStateChange()`.

You may call **Instrument** verbs during a test sequence.
- `InstrumentTab()` instruments an existing tab.
- `InstrumentNextTab()` instruments the next tab to be added to or opened in the
  specified browser.
- `AddInstrumentedTab()` adds a new tab to a browser and instruments it.
- `InstrumentNonTabWebContents()` instruments a piece of primary or secondary UI
  that uses a `WebView` and is not a tab (e.g. the tablet tabstrip or Tab Search
  dialog).

#### Specifying DOM Elements

Certain verbs that operate on instrumented WebContents take a `DeepQuery`, which
provides a path to a DOM element in the WebContents. A `DeepQuery` is a sequence
of one or more element selectors, as taken by the JavaScript `querySelector()`
method. A `DeepQuery` works as follows:

```js
let cur = document;
for (let selector of deepQuery) {
  if (cur.shadowRoot)
    cur = cur.shadowRoot;
  cur = cur.querySelector(selector);
}
```

If at any point the selector fails, the target DOM element is determined not to
exist. Often, this fails the test, but might not in all cases.

There is a strong preference to keep DeepQueries as simple as possible, both in
number of queries and in complexity of each query, in order to avoid tests being
fragile to small changes in page structure.
 - Use one query string for each Shadow Dom to pierce plus one query for the
   final element.
 - Most query strings can be a single element name or HTML id (e.g.
   "my-subcomponent" or "#enableButton"), only specify intervening elements if
   it's necessary to find the one you care about.

### Automatic Conversion

The following convenience methods are provided to convert a `TrackedElement*` to
a more specific object, primarily used in functions supplied to `WithElement()`
or one of the **After** verbs:
- `AsView<T>()` - converts the element to a view of the specific type; fails if
  it is not
- `AsInstrumentedWebContents()` - converts the element to an instrumented
  `WebContents`; fails if it is not

Example:
```cpp
  WithElement(kComboBoxId, [](ui::TrackedElement* el) {
    // Could have also used SelectDropdownItem for this:
    AsView<ComboBox>(el)->SelectItem(1);
  }),
```

### Custom Verbs

Sometimes you will have some common step or check (or set of steps and checks)
that you want to duplicate across a number of different test cases in your test
fixture. You can create a custom verb, which is just a method that returns a
`StepBuilder` or `MultiStep`. This method can combine existing verbs with steps
you create yourself, in any combination. To combine multiple steps, use the
`Steps()` method.

Here's an example of a very common custom verb pattern:

```cpp
// My test fixture class with a custom verb.
class MyHistoryTest : public InteractiveBrowserTest {

  // This custom verb will be used across multiple test cases.
  auto OpenHistoryPageInNewTab() {
    return Steps(
        InstrumentNextTab(kHistoryPageTabId),
        PressButton(kNewTabButton),
        PressButton(kAppMenuButton),
        SelectMenuItem(kHistoryMenuItem),
        SelectMenuItem(kOpenHistoryPageMenuItem),
        WaitForWebContentsNavigation(kHistoryPageTabId,
                                     chrome::kHistoryPageUrl));
  }
};

// An example test case.
IN_PROC_BROWSER_TEST_F(MyHistoryTest, NavigateTwoPagesAndCheckHistory) {
  InstrumentTab(browser(), kPrimaryTabId);
  RunTestSequence(
    WaitForWebContentsReady(kPrimaryTabId),
    NavigateWebContents(kPrimaryTabId, kUrl1),
    NavigateWebContents(kPrimaryTabId, kUrl2),
    // The custom verb sits happily in the action sequence.
    OpenHistoryPageInNewTab(),
    // We'll hand-wave the implementation of this method for now.
    WaitForStateChange(kHistoryPageTabId,
                       HistoryEntriesPopulated(kUrl1, kUrl2)));
}
```

### Custom Callbacks and Checks

Another common pattern is having a check that you want to perform over and over;
for example, checking that a histogram entry was added. This can absolutely be
done through a custom verb, however, perhaps you instead want to use it in an
`AfterShow()` step. In this case you can create a function that binds and
returns the appropriate callback.

```cpp
class MyDialogTest : public InteractiveBrowserTest {
  auto ExpectHistogramCount(const char* histogram_name, size_t expected_count) {
    return base::BindLambdaForTesting([histogram_name, expected_count, this](){
      EXPECT_EQ(expected_count, GetHistogramCount(histogram_name));
    });
  }
};

IN_PROC_BROWSER_TEST_F(MyDialogTest, ShowIncrementsHistogram) {
  RunTestSequence(
    PressButton(kShowMyDialogButtonId),
    AfterShow(kMyDialogContentsId,
              ExpectHistogramCount(kDialogShownHistogramName, 1)));
}
```

This could have also been implemented with a `WaitForShow()` and a custom verb
with a `Check()` or `CheckResult()`. Whether you use a custom callback or a
custom verb is up to you; do whatever makes your test easiest to read!

## Custom Test Fixtures

Most Kombucha tests will derive directly from either `InteractiveViewsTest` or
`InteractiveBrowserTest`.

If your test needs to derive from a different/custom test fixture class but you
would still like access to the Kombucha API, use `InteractiveViewsTestT<T>` or
`InteractiveBrowserTestT<T>` instead.

Example:
```cpp
// Want Kombucha functionality, but already have an existing test
// `MyCustomBrowserTest` with logic we need.
using MyTestFixture = InteractiveBrowserTestT<MyCustomBrowserTest>;

// Here's another way to do the same thing, if we want to further extend the
// test class.
class MyTestFixture2 : public InteractiveBrowserTestT<MyCustomBrowserTest> {
 public:
  MyTestFixture2();
  ~MyTestFixture2() override;

  // Add anything else we need here.
};
```

## Helper Classes

Kombucha helper classes are older, lower-level APIs that have been repurposed
to support interactive testing:
- `InteractionTestUtil`, `InteractionTestUtilView`,
  `InteractionTestUtilBrowser` - provide common UI functionality like pressing
  buttons, selecting menu items, and taking screenshots.
- `InteractionTestUtilMouse` - provides a way to inject mouse input, including
  clicking and dragging, into interactive tests.
- `WebContentsInteractionTestUtil` - provides a way to gain control of a
  WebContents, inject code, trigger and wait for navigation, and check and wait
  for changes in the DOM.

You should only rarely have to use these classes directly; if you do, it's
likely that Kombucha is missing some common verb that would cover your use case.
Please reach out to us!

## Changelog

### March 2023

Quality of life improvements:
 - You can now add Kombucha API to existing test fixtures using the following
   template mix-ins.
    - This removes the need for a lot of boilerplate when adding
      `InteractiveBrowserTestApi`.
    - See [Custom Test Fixtures](#custom-test-fixtures) for more info.
 - `base::BindOnce()` and `base::BindLambdaForTesting()` are no longer required
    in many cases.
     - This makes many steps less verbose - and less highly-indented - than they
       were previously.
     - See [Test Functions and Callbacks](#test-functions-and-callbacks) for
       more info.

New control-flow features (see [Control Flow](#control-flow) for more info):
 - `InParallel` runs several subsequences at once.
 - `If`, `IfMatches`, `IfView`, etc. conditionally run a subsequence.
    - Also added the ability to specify an optional "else" clause.

Bugfixes:
 - Slightly improved drag-handling on ChromeOS.

## Known Issues and Incompatibilities

The following will generate an error unless
[explicitly handled](#handling-incompatibilities):
 - `ActivateSurface()` does not work on the `linux-wayland` buildbot unless the
   surface is already active, due to vanilla Wayland not supporting programmatic
   window activation.
 - `Screenshot()` currently only works in specific pixel test jobs on the
   `win-rel` buildbot.

The following may produce unexpected or inconsistent behavior:
 - When `ClickMouse()` is used on Mac with right mouse button, events are sent
   asynchronously to avoid getting caught in a context menu run loop.
    - Most tests should still function normally, but be aware of the behavioral
      difference.
 - `DragMouse()` or `MoveMouseTo()` on Windows may result in Windows entering a
   drag loop that may hang or otherwise impact the test.
    - Tab dragging works as expected but other drag tests may be flaky or fail
      on the platform.

## Upcoming Features

To be supported in the near-future:
 - Touch input on ChromeOS
 - Touch input on Windows
 - More reliable drag-drop on Windows
