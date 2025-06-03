// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_RAWPTRMANUALPATHSTOIGNORE_H_
#define TOOLS_CLANG_PLUGINS_RAWPTRMANUALPATHSTOIGNORE_H_

// Array listing regular expressions of paths that should be ignored when
// running the rewrite_raw_ptr_fields tool on Chromium sources.
//
// If a source file path contains any of the lines in the filter file below,
// then such source file will not be rewritten.
//
// Lines prefixed with "!" can be used to force include files that matched a
// file path to be ignored.
//
// Note that the rewriter has a hardcoded logic for a handful of path-based
// exclusions that cannot be expressed as substring matches:
// - Excluding paths containing "third_party/", but still covering
//   "third_party/blink/"
//   (see the isInThirdPartyLocation AST matcher in RewriteRawPtrFields.cpp).
// - Excluding paths _starting_ with "gen/" or containing "/gen/"
//   (i.e. hopefully just the paths under out/.../gen/... directory)
//   via the isInGeneratedLocation AST matcher in RewriteRawPtrFields.cpp.
constexpr const char* const kRawPtrManualPathsToIgnore[] = {
    // Exclude to prevent PartitionAlloc<->raw_ptr<T> cyclical dependency.
    "base/allocator/",

    // Exclude dependences of raw_ptr.h
    // TODO(bartekn): Update the list of dependencies.
    "base/logging.h",
    "base/synchronization/lock_impl.h",
    "base/check.h",

    // win:pe_image target that uses this file does not depend on base/.
    "base/no_destructor.h",

    // Can't depend on //base, pointers/references under this directory can't be
    // rewritten.
    "testing/rust_gtest_interop/",

    // Exclude - deprecated and contains legacy C++ and pre-C++11 code.
    "ppapi/",

    // Exclude tools that do not ship in the Chrome binary. Can't depend on
    // //base.
    "base/android/linker/",
    "chrome/chrome_cleaner/",
    "tools/",
    "net/tools/",
    "chrome/chrome_elf/",
    "chrome/installer/mini_installer/",
    "testing/platform_test.h",

    // DEPS prohibits includes from base/
    "chrome/install_static",
    "net/cert/pki",
    "sandbox/mac/",

    // Exclude pocdll.dll as it doesn't depend on //base and only used for
    // testing.
    "sandbox/win/sandbox_poc/pocdll",

    // Exclude internal definitions of undocumented Windows structures.
    "sandbox/win/src/nt_internals.h",

    // Exclude directories that don't depend on //base, because nothing there
    // uses
    // anything from /base.
    "sandbox/linux/system_headers/",
    "components/history_clusters/core/",
    "ui/qt/",

    // The folder holds headers that are duplicated in the Android source and
    // need to
    // provide a stable C ABI. Can't depend on //base.
    "android_webview/public/",

    // Exclude dir that should hold C headers.
    "mojo/public/c/",

    // Exclude code that only runs inside a renderer process - renderer
    // processes are excluded for now from the MiraclePtr project scope,
    // because they are sensitive to performance regressions (to a much higher
    // degree than, say, the Browser process).
    //
    // Note that some renderer-only directories are already excluded
    // elsewhere - for example "v8/" is excluded in another part of this
    // file.
    //
    // The common/ directories must be included in the rewrite as they contain
    // code
    // that is also used from the browser process.
    //
    // Also, note that isInThirdPartyLocation AST matcher in
    // RewriteRawPtrFields.cpp explicitly includes third_party/blink
    // (because it is in the same git repository as the rest of Chromium),
    // but we go ahead and exclude most of it below (as Renderer-only code).
    "/renderer/",                     // (e.g. //content/renderer/ or
                                      // //components/visitedlink/renderer/
                                      //  or //third_party/blink/renderer)",
    "third_party/blink/public/web/",  // TODO: Consider renaming this directory
                                      // to",
                                      // public/renderer?",

    // Moved from //third_party/blink/renderer/platform/image-decoders/
    "components/image_decoders/",

    // Contains sysroot dirs like debian_bullseye_amd64-sysroot/ that are not
    // part of the repository.
    "build/linux/",

    // glslang_tab.cpp.h uses #line directive and modifies the file path to
    // "MachineIndependent/glslang.y" so the isInThirdPartyLocation() filter
    // cannot
    // catch it even though glslang_tab.cpp.h is in third_party/
    "MachineIndependent/",
};

#endif  // TOOLS_CLANG_PLUGINS_RAWPTRMANUALPATHSTOIGNORE_H_
