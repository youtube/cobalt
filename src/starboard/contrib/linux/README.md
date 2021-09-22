### Example Linux variant for Stadia

## ./linux/stadia
  This directory must be copied into starboard/linux/ in order to be built. Once this directory is copied into starboard/linux, you will be able to run the build commands for a Stadia configuration.
  Sample commands:

```bash
$ cp -r starboard/contrib/linux/stadia starboard/linux/
$ ./cobalt/build/gyp_cobalt linux-stadia
$ ninja -C out/linux-stadia_devel cobalt
```

**NOTE: This target will not be able to successfully link without a shared library that implements the shared interface defined in ./clients**

This build configuration was based completely on the linux-x64x11 configuration,
and the only differences are that this configuration extends ApplicationX11 in
order to initialize Stadia and that this configuration registers the Stadia
extension API in SbSystemGetExtension.
