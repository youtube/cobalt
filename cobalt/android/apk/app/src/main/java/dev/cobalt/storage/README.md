The StorageProto java class was generated using the protoc
compiler from the third_party/protobuf package.

Example:
 out/linux-x64x11_debug/protoc cobalt/storage/store/storage.proto \
     --java_out=starboard/android/apk/app/src/main/java

The code doesn't compile cleanly and produces warning:
 warning: [unchecked] unchecked conversion

To suppress the warning the following annotation should be added
to the StorageProto class:

@SuppressWarnings("unchecked")
