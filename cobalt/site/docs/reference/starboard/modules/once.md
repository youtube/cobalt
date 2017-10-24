---
layout: doc
title: "Starboard Module Reference: once.h"
---

Onces represent initializations that should only ever happen once per
process, in a thread-safe way.

## Macros

<div id="macro-documentation-section">

<h3 id="sb_once_initialize_function" class="small-h3">SB_ONCE_INITIALIZE_FUNCTION</h3>

Defines a function that will initialize the indicated type once and return
it. This initialization is thread safe if the type being created is side
effect free.<br>
These macros CAN ONLY BE USED IN A CC file, never in a header file.<br>
Example (in cc file):
SB_ONCE_INITIALIZE_FUNCTION(MyClass, GetOrCreateMyClass)
..
MyClass* instance = GetOrCreateMyClass();
MyClass* instance2 = GetOrCreateMyClass();
DCHECK_EQ(instance, instance2);

</div>

## Functions

### Init

**Declaration**

```
    static void Init() {                                   \
      s_singleton = new Type();                            \
    }                                                      \
  };                                                       \
  SbOnce(&s_once_flag, Local::Init);                       \
  return s_singleton;                                      \
}
```

### SbOnce

**Description**

Thread-safely runs `init_routine` only once.
<ul><li>If this `once_control` has not run a function yet, this function runs
`init_routine` in a thread-safe way and then returns `true`.
</li><li>If <code>SbOnce()</code> was called with `once_control` before, the function returns
`true` immediately.
</li><li>If `once_control` or `init_routine` is invalid, the function returns
`false`.</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbOnce-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbOnce-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbOnce-declaration">
<pre>
SB_EXPORT bool SbOnce(SbOnceControl* once_control,
                      SbOnceInitRoutine init_routine);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbOnce-stub">

```
#include "starboard/once.h"

bool SbOnce(SbOnceControl* /*once_control*/,
            SbOnceInitRoutine /*init_routine*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbOnceControl*</code><br>
        <code>once_control</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbOnceInitRoutine</code><br>
        <code>init_routine</code></td>
    <td> </td>
  </tr>
</table>

### s_once_flag 

**Description**

Defines a function that will initialize the indicated type once and return
it. This initialization is thread safe if the type being created is side
effect free.<br>
These macros CAN ONLY BE USED IN A CC file, never in a header file.<br>
Example (in cc file):
SB_ONCE_INITIALIZE_FUNCTION(MyClass, GetOrCreateMyClass)
..
MyClass* instance = GetOrCreateMyClass();
MyClass* instance2 = GetOrCreateMyClass();
DCHECK_EQ(instance, instance2);

**Declaration**

```
  static SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;  \
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>= SB_ONCE_INITIALIZER; </code><br>
        <code>\</code></td>
    <td> </td>
  </tr>
</table>

### s_singleton 

**Declaration**

```
  static Type* s_singleton = NULL;                         \
  struct Local {                                           \
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>= NULL;                        </code><br>
        <code>\</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>struct Local {                                          </code><br>
        <code>\</code></td>
    <td> </td>
  </tr>
</table>

