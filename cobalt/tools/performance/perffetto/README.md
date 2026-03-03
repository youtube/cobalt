# Memory Analysis on Perffetto

The goal is to analyze if a version of Kimono has memory usage. If the usage incrased,
find out the culprit.

## Make the Application debuggable

In this config file, `java/com/google/android/apps/youtube/tv/AndroidManifest.xml`

```
  <application
      android:allowBackup="true"
      android:banner="@drawable/app_banner"
      android:logo="@mipmap/ic_app"
      android:icon="@mipmap/ic_launcher"
      android:label="@string/app_name"
      android:name="com.google.android.apps.youtube.tv.application.TvRoot_Application"
      android:debuggable="true"
      android:extractNativeLibs="true">
```

## Build Kimono

```
$ build_kimono.sh
```

## Single Run

Run Kimono for several minutes to get the trace.

```
$ run.sh -p
```

## Perffetto

```
$ trace_processor --query-file process.sql out/trace.pftrace
...
"callsite_id","size_mb","deep_cobalt_address","entry_cobalt_address"
3607,25,"[NULL]","[NULL]"
3436,25,"[NULL]","[NULL]"
3190,25,"[NULL]","[NULL]"
2695,21,"[NULL]","0x35a859b"
4158,19,"[NULL]","[NULL]"
2677,12,"[NULL]","0x1e39b15"
...
```

If the memory entry has cobalt related stack, the addr will get printed here.

## Get the stack

```
addr2line -fCp -e ./libchrobalt.so ADDRESS
...
result may like this:
std::__Cr::deque<starboard::scoped_refptr<starboard::InputBuffer>, std::__Cr::allocator<starboard::scoped_refptr<starboard::InputBuffer> > >::push_back(starboard::scoped_refptr<starboard::InputBuffer> const&)
opus_audio_decoder.cc:?
```
