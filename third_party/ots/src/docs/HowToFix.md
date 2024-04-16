# How To Fix

This file describes the causes of common problems that OTS detects. 

### DFLT script doesn't satisfy the spec

    DFLT script doesn't satisfy the spec. DefaultLangSys is NULL
    DFLT script doesn't satisfy the spec. LangSysCount is not zero: $lang_sys_count

These errors are related to the language system declaration and how the lookup language tags are declared in the feature syntax.

Old files made in FontLab Studio version 5.0, using Adobe FDK version 2.5 or so, then the `latin` language was the default, and thus could go unspecified. 
And sometimes a language tag was declared after the untagged feature.
This was a common practice to assume that script `latn` is the default script. 
For example:

```
feature liga {
   sub f f i by ffi;
   sub f f l by ffl;
   sub f f by ff;
   sub f i by fi;
   sub f l by fl;
   language AZE;
   language CRT;
   language MOL;
   language ROM;
   language TRK;
   script DFLT;
   sub f f i by ffi;
   sub f f l by ffl;
   sub f f by ff;
   sub f i by fi;
   sub f l by fl;
} liga;
```

But the script `latn` should be declared before language tags. 
For example,

```
feature liga {
   script DFLT;
   sub f f i by ffi;
   sub f f l by ffl;
   sub f f by ff;
   sub f i by fi;
   sub f l by fl;
   script latn;
   sub f f i by ffi;
   sub f f l by ffl;
   sub f f by ff;
   sub f i by fi;
   sub f l by fl;
   language AZE;
   language CRT;
   language MOL;
   language ROM;
   language TRK;
} liga;
```

For fonts where sources are missing, or rebuilding is undesirable, there is a FontTools [script](https://github.com/fonttools/fonttools/blob/master/Snippets/fix-dflt-langsys.py) that can be used to hotfix the binary files.
