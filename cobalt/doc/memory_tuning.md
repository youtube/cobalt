# Memory Tuning #

Cobalt is designed to choose sensible parameters for memory-related options and
parameters through a system called "AutoMem".

On startup, AutoMem will print a memory table to the output console detailing
the memory allocations that will be assigned to the various subsystems in
cobalt.

Some settings will be "fixed" while others will be "flexible" so that their
memory consumption will scale down for memory constrained platforms.

Read on for more information.

**IMPORTANT**
*Setting `--max_cobalt_cpu_usage` and `--max_cobalt_gpu_usage` on the
command line is a beta feature.*

### Memory Settings Table ###

A table similar to the one below, will be printed on startup.

~~~
AutoMem:
 _______________________________________________________________________________
|SETTING NAME                          |VALUE        |         |TYPE  |SOURCE   |
| encoded_image_cache_size_in_bytes    |     1048576 |  1.0 MB |  CPU |   Build |
| image_cache_size_in_bytes            |    10485760 | 10.0 MB |  GPU | AutoSet |
| offscreen_target_cache_size_in_bytes |     2097152 |  2.0 MB |  GPU | AutoSet |
| remote_typeface_cache_size_in_bytes  |     4194304 |  4.0 MB |  CPU |   Build |
| skia_atlas_texture_dimensions        | 2048x2048x2 |  8.0 MB |  GPU | AutoSet |
| skia_cache_size_in_bytes             |     4194304 |  4.0 MB |  GPU |   Build |
| software_surface_cache_size_in_bytes |         N/A |     N/A |  N/A |     N/A |
|______________________________________|_____________|_________|______|_________|

~~~
This table shows the breakdown of how much memory is being allocated to each
sub-system, the type, and where it came from.

**SETTING NAME:** This is the name of the memory setting. If a setting can be
manually set through the command line or the build system, then it will be
accessible by using this name. For example adding the command line argument
`--image_cache_size_in_bytes=25165824` will manually set the Image Cache Size to
24 megabytes. Also note that this is also equivalent:
`--image_cache_size_in_bytes=24MB`. Note that the numerical value can include
the suffix kb/mb/gb to specify kilo/mega/giga-bytes. The numerical value can
be a floating point value. For example `--image_cache_size_in_bytes=.1GB` is
equivalent to `--image_cache_size_in_bytes=100MB`.

**VALUE:** This two column value has a first setting that describes what the
actual value is, and the second column is the amount of memory that the setting
consumes. This first setting gives hints on what kind of values the
setting can be set to via the command line. For example,
`skia_atlas_texture_dimensions` accepts texture sizes on the command line, such
as: `--skia_atlas_texture_dimensions=2048x4096x2`

**TYPE:** This specifies whether the setting consumes GPU or CPU memory.
For example, the Image Cache will decode images to buffers to the GPU memory
and therefore it is the classified as the GPU memory type.

**SOURCE:** This specifies where the memory setting came from. It will either
be set from a specific place or automatically generated from Cobalt.
  * Values for **SOURCE**:
    * `Starboard API`
      * The value used was reported by the result of a Starboard API function call.
      * Example: `SbSystemGetUsedCPUMemory()`
    * `Build`
      * Specified by the platform specific `*.gyp(i)` build file.
      * For example: see `image_cache_size_in_bytes` in [`build/config/base.gypi`](../build/config/base.gypi)
    * `CmdLine`
      * Read the memory setting value from the command line.
      * For example: `cobalt --image_cache_size_in_bytes=24MB`.
    * `AutoSet`
      * No value was specified and therefore Cobalt calculated the default value
	    automatically based on system parameters. For example many caches
		will be chosen proportionally to the size of the UI resolution.
    * `AutoSet (Constrained)`
      * This value was AutoSet to a default value, but then was reduced in
      response to `max_cobalt_cpu_usage` or `max_cobalt_gpu_usage being` set too low.

### Maximum Memory Table ###

This second table is also printed at startup and details the sum of memory and
maximum memory limits as reported by cobalt.

~~~
 MEMORY                 SOURCE          TOTAL      SETTINGS CONSUME
 ____________________________________________________________________
|                      |               |          |                  |
| max_cobalt_cpu_usage | Starboard API | 256.0 MB |         131.0 MB |
|______________________|_______________|__________|__________________|
|                      |               |          |                  |
| max_cobalt_gpu_usage | Starboard API | 768.0 MB |         124.0 MB |
|______________________|_______________|__________|__________________|
~~~

This table shows the limits for CPU and GPU memory consumption and also how
much memory is being consumed for each memory type.

**MEMORY**: This is the name of the memory limit. If you want to change this
setting manually then use the name on the command line. For example
`--max_cobalt_cpu_usage=150MB` will set Cobalt to 150MB limit for CPU
memory. If the sum of CPU memory exceeds this limit then memory settings of the
same type will reduce their memory usage.

**SOURCE**: This value indicates where the value came from.
 * `Starboard API`
   * `max_cobalt_cpu_usage`: This value was found from SbSystemGetTotalCPUMemory().
   * `max_cobalt_gpu_usage`: This value was found from SbSystemGetTotalGPUMemory().
 * `CmdLine`
   * `max_cobalt_cpu_usage`: --max_cobalt_cpu_usage was used as a command argument.
   * `max_cobalt_gpu_usage`: --max_cobalt_gpu_usage was used as a command argument.
 * `Build`
   * `max_cobalt_cpu_usage`: max_cobalt_cpu_usage was specified in a platform gyp file.
   * `max_cobalt_gpu_usage`: max_cobalt_gpu_usage was specified in a platform gyp file.

**TOTAL**: Represents the maximum available memory for settings. This value
came from **SOURCE**.

**SETTINGS CONSUME**: This value indicates the consumption of memory for the
current memory type.

For `max_cobalt_cpu_usage`, `Starboard API` indicates that this value came from
`SbSystemGetTotalCPUMemory()`  If this source value is `Starboard API` then this
value came from `SbSystemGetTotalCPUMemory()` (for CPU) or
`SbSystemGetTotalGPUMemory()` for GPU).

If the available memory for the Cobalt is less than the amount of memory
consumed by the settings, then any settings that are AutoSet AND adjustable
will reduce their memory consumption. When this happens, look for the string
*`AutoSet (Constrained)`* in the first table.

## Setting Maximum Memory Values ##

The max cpu and gpu memory of the system can be set either by command line or
by modifying the gyp build file.

Command Line:
  * `--max_cobalt_cpu_usage=160MB`
  * `--max_cobalt_gpu_usage=160MB`

Build settings:
  * `starboard/<PLATFORM>/gyp_configuration.gypi`
    * `max_cobalt_cpu_usage`
    * `max_cobalt_gpu_usage`

Command Line settings will override build settings.

### Memory Scaling ###

There are two primary ways in which the memory consumption settings will scale down.
One is by specifying `--max_cobalt_cpu_usage` (or `max_cobalt_gpu_usage`) to a
particular value (e.g. `--max_cobalt_cpu_usage=160MB`).

`--max_cobalt_cpu_usage` (and `--max_cobalt_gpu_usage`) will trigger the memory
to scale down whenever the memory settings memory consumption exceed the maximum
**TOTAL** value. The memory settings will be scaled down until their consumption is
less than or equal the maximum allowed value **TOTAL**. See also **SETTINGS CONSUME**.

*Forcing a Memory Setting to be flexible*

If a memory setting is set via a build setting, then it's possible to make it
flexible via the command line by setting the value to "autoset". For example,
 `--image_cache_size_in_bytes=auto` will allow `image_cache_size_in_bytes` to be
flexible by disabling the value being set by a build setting.

### Memory Warnings ###

Cobalt will periodically check to see if the memory consumed by the application
is less than the `--max_cobalt_cpu_usage` and `--max_cobalt_gpu_usage` amount.
If the cpu/gpu exceeds this maximum value then an error message will be logged
once to stdout for cpu and/or gpu memory systems.


### Example 1 - Configuring for a memory restricted platform ###

Let's say that we are configuring platform called "XXX":

We will configure XXX such that:
  * `image_cache_size_in_bytes` will be set to 32MB in the build settings.
  * `skia_atlas_texture_dimensions` will be set to `2048x2048x2` in the build settings.
  * `max_cobalt_cpu_usage` will be set to 160MB on the command line.

**Configuring `image_cache_size_in_bytes` to be 32MB:**
  * in `starboard\<PLATFORM>\gyp_configuration.gypi`
    * add `'image_cache_size_in_bytes': 32 * 1024 * 1024,`

**Configuring `skia_atlas_texture_dimensions` to be 2048x2048x2:**

  * in `src\starboard\XXX\gyp_configuration.gypi`
    * add `'skia_glyph_atlas_width': '2048'`
    * add `'skia_glyph_atlas_height': '2048'`
    * (note that the third dimension is assumed)

**Configuring `max_cobalt_cpu_usage` to be 160MB:**

  * `cobalt --max_cobalt_cpu_usage=160MB`

### Example 2 - Configuring for a memory-plentiful platform ###

The following command line will give a lot of memory to image cache and give
500MB to `max_cobalt_cpu_usage` and `max_cobalt_gpu_usage`.

~~~
cobalt --max_cobalt_cpu_usage=500MB --max_cobalt_gpu_usage=500MB
--image_cache_size_in_bytes=80MB
~~~

## API Reference ##

#### Memory System API ####

  * `max_cobalt_cpu_usage`
    * This setting will set the maximum cpu memory that the app will consume.
      CPU Memory settings will scale down their consumption in order to stay under
      the `max_cobalt_cpu_usage`. If memory consumption exceeds this value during
      runtime then a memory warning will be printed to stdout.
    * Set via command line or else build system or else starboard.
      * starboard value will bind to `SbSystemGetTotalCPUMemory()`.
  * `max_cobalt_gpu_usage`
    * This setting will set the maximum gpu memory that the app will consume.
      GPU Memory settings will scale down their consumption in order to stay under
      the `max_cobalt_gpu_usage`. If memory consumption exceeds this value during
      runtime then a memory warning will be printed to stdout.
    * Set via command line or else build system or else starboard.
      * starboard value will bind to `SbSystemGetTotalGPUMemory()`.
    * Note that `SbSystemGetTotalGPUMemory()` is optional. If no value exists
      for `max_cobalt_gpu_usage` in build/commandline/starboard settings then no
      GPU memory checking is performed.

#### Memory Setting API ####

  * `image_cache_size_in_bytes`
    * See documentation *Image cache capacity* in `performance_tuning.md` for what
      this setting does.
    * Set via command line, or else build system, or else automatically by Cobalt.
  * `remote_typeface_cache_size_in_bytes`
    * Determines the capacity of the remote typefaces cache which manages all typefaces
      downloaded from a web page.
    * Set via command line, or else build system, or else automatically by Cobalt.
  * `skia_atlas_texture_dimensions`
    * Determines the size in pixels of the glyph atlas where rendered glyphs are
      cached. The resulting memory usage is 2 bytes of GPU memory per pixel.
      When a value is used that is too small, thrashing may occur that will
      result in visible stutter. Such thrashing is more likely to occur when CJK
      language glyphs are rendered and when the size of the glyphs in pixels is
      larger, such as for higher resolution displays.
      The negative default values indicates to the Cobalt that these settings
      should be automatically set.
    * Set via command line, or else build system, or else automatically by Cobalt.
    * Note that in the gyp build system, this setting is represented as two values:
      * `skia_glyph_atlas_width` and
      * `skia_glyph_atlas_height`
  * `skia_cache_size_in_bytes`
    * See documentation *Glyph atlas size* in `performance_tuning.md` for what this
      setting does.
    * Set via command line, or else build system or else automatically by Cobalt.
  * `software_surface_cache_size_in_bytes`
    * See documentation *Scratch Surface cache capacity* in `performance_tuning.md`
      for what this setting does.
    * Set via command line, or else build system, or else automatically by Cobalt.

#### Units for Command Line Settings ####

Memory values passed into Cobalt via command line arguments support units such
kb, mb, and gb for kilo-byte, megabyte, gigabytes. These units are case insensitive.

For example, these are all equivalent on the command line:

`--image_cache_size_in_bytes=67108864`
`--image_cache_size_in_bytes=65536kb`
`--image_cache_size_in_bytes=64mb`
`--image_cache_size_in_bytes=.0625gb`
