# Back Press Interception System on Android

This directory contains the central infrastructure and handlers for managing the
back gesture/button and the Escape key on Android.

## Overview
Android 13+ introduced the **Predictive Back Gesture**, which allows users to
preview the destination of the back gesture (such as the home screen or a
previous activity) before committing to it. For this system animation to work
correctly, Android needs to know *before* the gesture is executed whether the
application will intercept the back press.

To support this, Chrome uses a unified back press management system centered
around the `BackPressManager` and the `BackPressHandler` interface. Rather than
dynamically registering and unregistering callbacks with the OS, Chrome
registers a single high-level dispatcher callback. Individual features declare
their availability to intercept back events dynamically through reactive boolean
suppliers.

---

## Key Components

### 1. `BackPressHandler`
Every feature that needs to intercept the back press must implement
`BackPressHandler`. It defines the following key methods:

*   **`getHandleBackPressChangedSupplier()`**: Returns a
    `NonNullObservableSupplier<Boolean>`. The supplier **must** yield `true`
    when the feature is active and wants to consume the back gesture (e.g. a
    bottom sheet is expanded, a popup is visible, or the tab has navigation
    history), and `false` otherwise.
*   **`handleBackPress()`**: Executed when a back gesture is completed. The
    handler must consume the gesture (e.g. dismiss the UI) and return
    `@BackPressResult int`.
    *   `BackPressResult.SUCCESS`: The handler successfully processed the back
        press.
    *   `BackPressResult.FAILURE`: Used only for debugging. In production, each
        handler must consume the back event. A `FAILURE` indicates a state
        synchronization bug (i.e., `getHandleBackPressChangedSupplier()`
        returned `true` when the feature was not actually in a state to
        intercept/consume the back press).
*   **Progressive Gesture Animations (Android 14+ / API 34+)**:
    *   `handleOnBackStarted(BackEventCompat)`
    *   `handleOnBackProgressed(BackEventCompat)`
    *   `handleOnBackCancelled()`
    These can be overridden to run custom transitions or animations matching
    the user's swipe progress.
*   **Escape Key Customization**:
    *   `invokeBackActionOnEscape()`: Returns `true` if the Escape key should
        behave exactly like the back button.
    *   `handleEscPress()`: Returns `true` if it handles the Escape key press
        with a custom action.

### 2. `BackPressManager`
`BackPressManager` is the central coordinator:
*   Maintains an array of `BackPressHandler` instances sorted by priority
    (defined by `BackPressHandler.Type`).
*   Observes the status suppliers of all registered handlers.
*   Enables the activity's `OnBackPressedCallback` only if at least one
    handler's supplier is `true`.
*   During a gesture (API 34+), it locks onto the highest priority enabled
    handler (`mActiveHandler`) at the start of the swipe, preventing glitches
    if the handler's supplier status flips during the swipe.

### 3. System Back / App Minimization
When no custom features want to intercept the back gesture, the app either
closes the current tab or minimizes to the background.
*   `MinimizeAppAndCloseTabBackPressHandler` sits at the lowest priority
    (`Type.MINIMIZE_APP_AND_CLOSE_TAB`).
*   On Android 12+ ("System Back"), if the only remaining action is to minimize
    the app, this handler yields `false` from its supplier. This disables
    Chrome's custom back callback, allowing the Android OS to handle the back
    gesture directly, enabling the system's predictive back-to-home animation.

---

## Priority Order
The handler priorities are defined in `BackPressHandler.Type`. A lower integer
value means higher priority. When multiple handlers are active simultaneously,
the one with the smallest type value intercepts the gesture.

Example order (high to low priority):
1.  `Type.TEXT_BUBBLE` (highest)
2.  `Type.BOTTOM_SHEET`
3.  `Type.TAB_HISTORY`
4.  `Type.MINIMIZE_APP_AND_CLOSE_TAB` (lowest)

---

## How to Add a New Back Press Handler

Follow these steps to register a new back press handler in Chrome:

### Step 1: Implement `BackPressHandler`
Implement the interface in your component. Typically, you will use a
`SettableNonNullObservableSupplier<Boolean>` to publish the active state of
your component:

```java
public class MyFeatureCoordinator implements BackPressHandler {
    private final SettableNonNullObservableSupplier<Boolean>
            mBackPressSupplier = ObservableSuppliers.createNonNull(false);

    public void show() {
        // Show UI...
        mBackPressSupplier.set(true);
    }

    @Override
    public NonNullObservableSupplier<Boolean>
            getHandleBackPressChangedSupplier() {
        return mBackPressSupplier;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return hide() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    private boolean hide() {
        if (!isShowing()) {
            return false;
        }
        // Hide UI...
        mBackPressSupplier.set(false);
        return true;
    }
}
```

### Step 2: Define your type in `BackPressHandler.java`
Open [BackPressHandler.java][1]:
1.  Add your new handler type to the `@IntDef` list at the top.
2.  Add the constant to the `Type` interface:
    ```java
    @interface Type {
        ...
        int MY_NEW_HANDLER = <priority_index>;
        ...
    }
    ```
    *Note: Shift the values of subsequent types down if you are inserting your
    handler in the middle of the priority list. Update `NUM_TYPES` so it matches
    `MINIMIZE_APP_AND_CLOSE_TAB + 1`.*

### Step 3: Register metrics map in `BackPressManager.java`
Open [BackPressManager.java][2]:
1.  Add your new handler to the static `sMetricsMap` initialization:
    ```java
    map.put(Type.MY_NEW_HANDLER, <next_available_uma_index>);
    ```
2.  Increment `sMetricsMaxValue` to account for the new index.

### Step 4: Append UMA enum entry
Open [enums.xml][3]:
1.  Locate the `<enum name="BackPressConsumer">` block.
2.  Append your handler at the end:
    ```xml
    <int value="<next_available_uma_index>" label="(<priority_index_padded>) MY_NEW_HANDLER"/>
    ```
    *E.g. `<int value="29" label="(05) CANCEL_TAB_SWITCHER_DRAG"/>`*

### Step 5: Register the handler with `BackPressManager`
Retrieve the `BackPressManager` instance in your activity or UI coordinator
initializer and call `addHandler`:

```java
mBackPressManager.addHandler(myFeatureCoordinator, Type.MY_NEW_HANDLER);
```

Ensure the handler is removed when the component is destroyed:
```java
mBackPressManager.removeHandler(myFeatureCoordinator);
```

---

## Best Practices
*   **Register Once**: Do not dynamically call `addHandler` and `removeHandler`
    repeatedly. Register the handler once on startup, and control its behavior
    by updating the value of the `ObservableSupplier<Boolean>` returned by
    `getHandleBackPressChangedSupplier()`.
*   **Accurate Suppliers**: Your supplier must strictly yield `true` ONLY when
    your handler is capable of consuming the back press. Failing to do so (i.e.
    returning `true` but having `handleBackPress()` return
    `BackPressResult.FAILURE` or do nothing) will result in broken back
    navigation behavior and assert in debug builds.
*   **Clean Up**: Always remove your handler instance from `BackPressManager`
    when the component or Activity is destroyed to prevent memory leaks.

[1]: https://cs.chromium.org/chromium/src/components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/gesture/BackPressHandler.java
[2]: https://cs.chromium.org/chromium/src/chrome/browser/back_press/android/java/src/org/chromium/chrome/browser/back_press/BackPressManager.java
[3]: https://cs.chromium.org/chromium/src/tools/metrics/histograms/metadata/android/enums.xml
