# Cobalt Splash Screen

## Startup splash screen sequence

There can be up to three splash screens shown when launching web applications on
Cobalt:

  * one from the system
  * one from Cobalt
  * one from the web application itself

The system splash screen is often a transition from the application icon
on the home screen to a static asset dictated by the platform (which is outside
of Cobalt's control). The Cobalt splash screen is shown as soon as Cobalt can
render until the web application is loaded. The web application splash screen is
the HTML content shown immediately upon loading the web application (this may
resemble a typical splash screen, but it really can be whatever the application
chooses to show on starting).

## Cobalt splash screen priority order

The Cobalt splash screen must be specified as a URL to a document. The document
must be either self-contained or all of its references must be local. That means
the document should not reference any external CSS, JavaScript, or image files,
for example. This simplifies the caching process so that only a single document
must be cached without tracing references. All fallback splash screens must
refer to local documents. This is so the fallback splash screen can be shown
without latency and even when there is no network available. Specifically, the
fallback splash screen URL and its references should start with either
`file:///` or `h5vcc-embedded://`. Additionally `none` can be used to specify
that no Cobalt splash screen should be constructed; the system splash sequence
transitions directly into the application splash sequence once the page is
loaded.

The Cobalt splash screen is one of the following, in order of precedence:

  1. **Web cached splash screen:** If a splash screen specified by a web
     application is cached from a previous instance of Cobalt, it will be loaded
     at startup. The key for the cache splash screen is based on host & path of
     the initial URL with no query or hash. If network connectivity is available
     at startup, when the initial web application URL is processed, a custom
     `rel="splashscreen"` attribute of the link element is used to specify and
     cache the splashscreen URL for future runs.

  2. **Command line fallback splash screen:** This is specified as a command
     line argument `--fallback_splash_screen_url` via the system and used when
     cache is unavailable.  This is the case when there is no local cache
     storage, cache has been cleared, or the application is started for the
     first time.

  3. **Build-time fallback splash screen:** If a web cached splash screen is
     unavailable and command line parameters are not passed by the system,
     a CobaltExtensionConfigurationApi fallback splash screen may be used.
     Porters should set the `CobaltFallbackSplashScreenUrl` value in
     `configuration.cc` to the splash screen URL.

  4. **Default splash screen:** If no web cached splash screen is available, and
     command line and CobaltExtensionConfigurationApi fallbacks are not set, a
     default splash screen will be used. This is set in
     `configuration_defaults.cc` to refer to a black splash screen.

## Web-updatability

Since Cobalt parses the link element's `rel="splashscreen"` attribute for the
splash screen URL in the content fetched from the initial URL, an application
developer may update the splash screen by changing that attribute in the link
element. On the next load of the application, the new splash screen will be
cached, and on the subsequent load of the application, the new cached splash
screen will be shown.

For example, the document at the initial URL could contain
```
<link rel="splashscreen" href="https://www.example.com/self-contained.html">
```
where `"https://www.example.com/self-contained.html"` is the address of some
self-contained splash screen document. The document must not violate the Content
Security Policy. The splash screen is treated as a script resource by the CSP.

### Caching implementation requirements

In order to cache the application-provided splash screen, Cobalt will attempt
to create directories and write files into the directory returned from a call to
`SbSystemGetPath(kSbSystemPathCacheDirectory, ...)`.  Cobalt will expect the
data that it writes into that directory to persist across process instances.
Cobalt will also need to read the cached splash screen from the cache directory
when starting up.

## Topic-specific splash screens

It is possible to specify multiple splash screens for a given Cobalt-based
application, using a start-up 'topic' to select between the available splash
screens. This can be useful when an application has multiple entry points that
require different splash screens. The topic may be specified in the start-up url
or deeplink as a query parameter. For example,
`https://www.example.com/path?topic=foo`. If a splash-screen has been specified
for topic 'foo', it will be used. Otherwise, the topic is ignored. Topic values
should be URL encoded and limited to alphanumeric characters, hyphens,
underscores, and percent signs.

There are three ways to specify topic-specific splash screens. These methods mirror
the types of splash screens listed above, and unless specified, the rules here
are the same as for non-topic-based splash screens.

  1. **Web cached splash screen:** A custom `rel="<topic>_splashscreen"`
     attribute on a link element is used to specify a topic-specific splash
     screen. There can be any number of these elements with different topics, in
     addition to the topic-neutral `rel="splashscreen"`.

  2. **Command line fallback splash screen:** The command line argument
     `--fallback_splash_screen_topics` can be used if the cache is unavailable.
     The argument accepts a list of topic/file parameters. If a file is not a
     valid URL path, then it will be used as a filename at the path specified by
     `--fallback_splash_screen_url`. For example,
     `foo_topic=file:///foo.html&bar=bar.html`.

  3. **Build-time fallback splash screen:** If a web cached splash screen is
     unavailable and command line parameters are not passed by the system, a
     CobaltExtensionConfigurationApi fallback splash screen may be used. Porters
     should set the `CobaltFallbackSplashScreenTopics` value in
     `configuration.cc` and this value should look like the command line option.

## Application-specific splash screens

On systems that plan to support multiple Cobalt-based applications, an
application developer may wish to use the command line arguments for the
fallback splash screen to display different Cobalt splash screens for different
applications. The logic for passing in these different command line arguments to
the Cobalt binary must be handled by the system.

Alternatively, an application developer may use the default black splash screen
whenever a cached splash screen is not available and rely on the web application
to specify an application-specific cached splash screen otherwise.

## Provided embedded resource splash screens
For convenience, we currently provide the following splash screens as embedded
resources:

  * `h5vcc-embedded://black_splash_screen.html` - a black splash screen
  * `h5vcc-embedded://cobalt_splash_screen.html` - a splash screen showing the
    Cobalt logo
