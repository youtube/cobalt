# Building Starboard

Starboard currently uses GYP as the basis of its build system, though there is
already some movement towards GN as a replacement.

While you can integrate Starboard into any build system, there are enough knobs
and dials that it would be a daunting effort. Instead, Starboard tries to provide a
functional build framework that an application developer can easily integrate with.


## The Flow

The basic flow of how Starboard builds is:

1. `starboard/build/gyp` - Parse command line parameters and pass them into
   `GypRunner`.
2. `starboard/build/gyp_runner.py`
   1. Load the `PlatformConfiguration`.
   2. Load the `ApplicationConfiguration`.
   3. Calculate and merge the GYP includes, GYP variables, environment
      variables, and generator variables, and pass them into GYP.
3. tools/gyp - Parse all the .gyp and included .gypi files, generate ninja.
4. ninja - Build all the source files.


## The Platform vs. the Application

In this documentation, you will see a lot of discussion about things being
Platform or Application concerns.

The Platform is more-or-less what you think it is -- Everything you might need
to build any Starboard Application on a given platform.

It helps to think about an Application as a broader concept than a single
program. From Starboard's build system's perspective, an Application is a
*single configuration of build variables per platform*, which may produce many
build targets beyond a single executable - shared libraries, tests, and so on.


## Application Customization

Each Application will probably want to define its own knobs, dials, and defaults
to be able to be customized per platform, and so Starboard provides a space for
the Application to do that.

Each Application can provide a platform-specific ApplicationConfiguration
instance by creating a python module in
`<platform-directory>/<application-name>/configuration.py`. This module must
contain a class definition that extends from
`starboard.build.application_configuration.ApplicationConfiguration`. If the
class is not found in the platform-specific location, a generic configuration
will be loaded, as dictated by the `APPLICATIONS` registry in
`starboard_configuration.py`.

Additionally, the Application can provide a GYPI file to be included at
`<platform-directory>/<application-name>/configuration.gypi`. This will, by
default, be included at the end of all other configuration GYPI files, but this
behavior can be arbitrarily customized by the loaded `ApplicationConfiguration`
class.


## HOWTO: Create a new Application

1. Create your Application's root `.gyp` file. Often called `all.gyp` or
   something similar.

2. Create a cross-platform `ApplicationConfiguration` python class for your
   application. Take a look at
   [`cobalt_configuration.py`](../../cobalt/build/cobalt_configuration.py) as an
   example.

   Define a subclass of
   `starboard.build.application_configuration.ApplicationConfiguration` and
   override any desired methods. In particular, you probably at least want to
   override the `GetDefaultTargetBuildFile()` method to point at your root
   `.gyp` file from step 1.

3. Register your Application in your `starboard_configuration.py` file in your
   source tree root.

   To do this, just add an entry to the `APPLICATIONS` Mapping that maps your
   canonical Application name to the cross-platform `ApplicationConfiguration`
   subclass constructor for your application.

       APPLICATIONS = {
           # ...

           'example_app': example_app.ExampleAppConfiguration

           # ...
       }

At this point, you should be able to build your application with:

    starboard/build/gyp -a <application-name>
