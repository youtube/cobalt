# Android Studio

[TOC]

## Usage

Make sure you have followed
[android build instructions](android_build_instructions.md) already.

```shell
build/android/gradle/generate_gradle.py --output-directory out/Debug
```

The above commands create a project dir `gradle` under your output directory.
Use `--project-dir <project-dir>` to change this.

To import the project:
* Use "Import Project", and select the directory containing the generated
  project, e.g. `out/Debug/gradle`.

See [android_test_instructions.md](testing/android_test_instructions.md#Using-Emulators)
for more information about building and running emulators.

Feel free to accept Android Studio's recommended actions. `generate_gradle.py`
should have already set up a working version of the gradle wrapper and the
android gradle plugin, as well as a default Android SDK location at
`~/Android/Sdk`. Since the same script needs to support various versions of
Android Studio, the defaults may have lower version than the one recommended by
your version of Android Studio. After you accept Android Studio's update actions
the `generate_gradle.py` script will try to keep the newer versions when it is
re-run.

You'll need to re-run `generate_gradle.py` whenever new directories containing
source files are added.

* After regenerating, Android Studio should prompt you to "Sync". If it
  doesn't, try some of the following options:
    * File -&gt; "Sync Project with Gradle Files"
    * Button with two arrows on the right side of the top strip.
    * Help -&gt; Find Action -&gt; "Sync Project with Gradle Files"
    * After `gn clean` you may need to restart Android Studio.
    * File -&gt; "Invalidate Caches / Restart..."

## How It Works

By default, only an `_all` module containing all java apk targets is generated.
If just one apk target is explicitly specified, then a single apk module is
generated.

If you really prefer a more detailed structure of gn targets, the deprecated
`--split-projects` flag can be used. This will generate one module for every gn
target in the dependency graph. This can be very slow and is no longer
supported.

### Generated files

Most generated .java files in GN are stored as `.srcjars`. Android Studio does
not support them. Our build will automatically extract them to a
`generated_java` directory in the output directory during the build. Thus if a
generated file is missing in Android Studio, build it with ninja first and it
should show up in Android Studio afterwards.

### Native Files

This option is deprecated and no longer supported since Android Studio is very
slow when editing in a code base with a large number of C++ files, and Chromium
has a lot of C++ code. It is recommended to use [VS Code](vscode.md) to edit
native files and stick to just editing java files in Android Studio.

If you still want to enable editing native C/C++ files with Android Studio, pass
in any number of `--native-target [target name]` flags in order to use it. The
target must be the full path and name of a valid gn target (no short-forms).
This will require you to install `cmake` and `ndk` when prompted. Accept Android
Studio's prompts for these SDK packages.

You need to disable a new gradle option in order to edit native files:
File -&gt; Settings -&gt; Experimental
-&gt; Gradle and uncheck "Only resolve selected variants".

This is not necessary, but to avoid "This file is not part of the project...",
you can either add an extra `--native-target` flag or simply copy and paste the
absolute path to that file into the CMakeLists.txt file alongside the existing
file paths. Note that changes to CMakeLists.txt will be overwritten on your next
invocation of `generate_gradle.py`.

Example:

```shell
build/android/gradle/generate_gradle.py --native-target //chrome/android:libchrome
```

## Tips

* Use environment variables to avoid having to specify `--output-directory`.
    * Example: Append `export CHROMIUM_OUT_DIR=out; export BUILDTYPE=Debug` to
      your `~/.bashrc` to always default to `out/Debug`.
* Using the Java debugger is documented
  [here](android_debugging_instructions.md#android-studio).
* Configuration instructions can be found
  [here](http://tools.android.com/tech-docs/configuration). One suggestions:
    * Launch it with more RAM:
      `STUDIO_VM_OPTIONS=-Xmx2048m /opt/android-studio-stable/bin/studio-launcher.sh`
* If you ever need to reset it: `rm -r ~/.config/Google/AndroidStudio*/`
* Import Chromium-specific style and inspections settings:
    * Help -&gt; Find Action -&gt; "Code Style" (settings) -&gt; Java -&gt;
      Scheme -&gt; Import Scheme
        * Select `tools/android/android_studio/ChromiumStyle.xml` -&gt; OK
    * Help -&gt; Find Action -&gt; "Inspections" (settings) -&gt;
      Profile -&gt; Import profile
        * Select `tools/android/android_studio/ChromiumInspections.xml` -&gt; OK
* Turn on automatic import:
    * Help -&gt; Find Action -&gt; "Auto Import"
        * Tick all the boxes under "Java" and change the dropdown to "All".
* Turn on documentation on mouse hover:
    * Help -&gt; Find Action -&gt; "Show quick documentation on mouse move"
* Turn on line numbers:
    * Help -&gt; Find Action -&gt; "Show line numbers"
* Turn off indent notification:
    * Help -&gt; Find Action -&gt; "Show notifications about detected indents"
* Format changed files (Useful for changes made by running code inspection):
    * Set up version control
        * File -&gt; Settings -&gt; Version Control
        * Add src directories
    * Commit changes and reformat
        * Help -&gt; Find Action -&gt; "Commit Changes"
        * Check "Reformat code" & "Optimize imports" and commit
* Change theme from GTK+ to another one to avoid invisible menus.
    * Help -&gt; Find Action -&gt; "Theme: Settings > Appearance"

### Useful Shortcuts

* `Shift - Shift`: Search to open file or perform IDE action
* `Ctrl + N`: Jump to class
* `Ctrl + Shift + T`: Jump to test
* `Ctrl + Shift + N`: Jump to file
* `Ctrl + F12`: Jump to method
* `Ctrl + G`: Jump to line
* `Shift + F6`: Rename variable
* `Ctrl + Alt + O`: Organize imports
* `Alt + Enter`: Quick Fix (use on underlined errors)
* `F2`: Find next error

### Building with Gradle

Gradle builds are not supported. Only editing is supported in Android Studio.
Use ninja to build as usual.

## Status

### What works

* Android Studio v2021~v2023.
* Java editing.
    * Application code in `main` sourceset.
    * Instrumentation test code in `androidTest` sourceset.
* Native code editing (deprecated, use [VS Code](vscode.md) instead).
* Symlinks to existing .so files in jniLibs (doesn't generate them).
* Editing resource xml files
* Layout editor (limited functionality).
* Java debugging (see
[here](/docs/android_debugging_instructions.md#Android-Studio)).
* Import resolution and refactoring across java files.
* Separate Android SDK for Android Studio.

### What doesn't work

* Building with Gradle.
* The "Make Project" button doesn't work.
    * Stick to using `autoninja` to build targets and just use Android Studio
      for editing java source files.
* No active work is underway or planned to expand Android Studio support.