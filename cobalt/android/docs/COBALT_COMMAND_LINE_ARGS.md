# How to Supply Command-Line Arguments to Cobalt

This guide explains the proper way to supply command-line arguments to Cobalt, including feature flags and other switches.

## The New Method (Recommended)

The recommended way to pass command-line arguments to Cobalt is to use a single `--esa` (extra string array) parameter with the key `commandLineArgs`. This method simplifies the process by allowing you to pass all your arguments in a single string, with different flags separated by commas and multiple values for a single flag separated by semicolons.

### Format

The format for the `commandLineArgs` string is as follows:

```
'key1=value1;value2;value3,key2=valueA;valueB,key3=valueC'
```

- **Flags are separated by commas (`,`)**: Each key-value pair is separated by a comma.
- **Values are separated by semicolons (`;`)**: If a flag has multiple values, they are separated by a semicolon.

### Examples

Here are some examples of how to use the new method with `adb`:

**1. Enabling a Single Feature**

To enable a single feature, you can use the following command:

```bash
adb shell am start --esa commandLineArgs 'enable-features=MyCoolFeature' dev.cobalt.coat/dev.cobalt.app.MainActivity
```

**2. Enabling Multiple Features**

To enable multiple features, separate them with a semicolon:

```bash
adb shell am start --esa commandLineArgs 'enable-features=MyCoolFeature;AnotherFeature;ThirdFeature' dev.cobalt.coat/dev.cobalt.app.MainActivity
```

**3. Disabling Features and Setting Other Flags**

You can combine multiple flags in a single command. For example, to disable a feature and set the `js-flags`:

```bash
adb shell am start --esa commandLineArgs 'disable-features=OldFeature;AnotherOldFeature,js-flags=--some-js-flag' dev.cobalt.coat/dev.cobalt.app.MainActivity
```

**4. A Complex Example**

Here is a more complex example that combines multiple flags and values:

```bash
adb shell am start --esa commandLineArgs 'enable-features=A;B;C,disable-features=D;E;F,js-flags=--flag1;--flag2,blink-enable-features=L;M;N' dev.cobalt.coat/dev.cobalt.app.MainActivity
```

**5. Enabling Features with Values**

The parser also supports passing values to individual features. This is useful for features that act as key-value pairs themselves.

```bash
adb shell am start --esa commandLineArgs 'enable-features=MyFeature=True;AnotherFeature=123' dev.cobalt.coat/dev.cobalt.app.MainActivity
```

This will result in the following flag being passed to Cobalt: `--enable-features=MyFeature=True,AnotherFeature=123`
