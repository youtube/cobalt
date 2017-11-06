# App Launchers

The app launcher framework is used to run an executable on a given platform,
allowing its output/results to be used by other scripts or tools.

## Making an App Launcher

In order to use this framework for your platform, there must be a method called
"GetLauncher()" in the PlatformConfig class that your platform's
"gyp_configuration.py" file refers to.  It should load and return a module
containing a class called "Launcher."  This class must inherit from the
AbstractLauncher class in [abstract_launcher.py](../../abstract_launcher.py),
and must implement at minimum the following abstract methods:

- Run(): Runs the executable, logs its output, and returns its return code.
         Generally, any app installation work is also done in this method.
- Kill(): Kills the currently running executable and cleans up any leftover
          resources such as threads, processes, etc.

Once the above steps are implemented, tools that use this framework, such as
[Starboard's unit test runner](../../testing/test_runner.py), should work
properly for your platform.  For an example of a Launcher class, see
[this Linux implementation](../../../linux/shared/launcher.py).  For an example
of the corresponding "GetLauncher()" method, see
[this gyp_configuration.py file](../../../linux/shared/gyp_configuration.py).

## Using an App Launcher

In order to use this framework in a Python tool, it must import
abstract_launcher.py and call "abstract_launcher.LauncherFactory()."  This
method returns a Launcher object from the platform specified by its "platform"
argument.  To run the launcher, call its "Run()" method, and to stop it, call
its "Kill()" method.  If your tools need to access the Launcher's output while
the executable is still running, have it start "Run()" in a separate thread;
this will allow the main thread to easily read from the Launcher's output file.
For an example of creating and using a Launcher, see
[this example](../../example/app_launcher_client.py).